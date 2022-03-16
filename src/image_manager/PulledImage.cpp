/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "PulledImage.hpp"

#include "common/PathRAII.hpp"
#include "common/Utility.hpp"
#include "image_manager/Utility.hpp"


namespace sarus {
namespace image_manager {

PulledImage::PulledImage(std::shared_ptr<const common::Config> config, const std::string& digest, const boost::filesystem::path& imagePath)
    : InputImage{std::move(config)},
      digest{digest},
      imageDir{imagePath}
{
    auto imageIndex = common::readJSON(imageDir.getPath() / "index.json");
    auto schemaItr = imageIndex.FindMember("schemaVersion");
    if (schemaItr == imageIndex.MemberEnd() || schemaItr->value.GetUint() != 2) {
        log("Unsupported OCI image index format. The 'schemaVersion' property could not be found or its value is different from '2'. "
            "Attempting to proceed with image processing...", common::LogLevel::WARN);
    }

    std::string manifestDigest = imageIndex["manifests"][0]["digest"].GetString();
    auto manifestHash = manifestDigest.substr(manifestDigest.find(":")+1);
    auto imageManifest = common::readJSON(imageDir.getPath() / "blobs/sha256" / manifestHash);

    std::string configDigest = imageManifest["config"]["digest"].GetString();
    auto configHash = configDigest.substr(configDigest.find(":")+1);
    auto imageConfig = common::readJSON(imageDir.getPath() / "blobs/sha256" / configHash);

    metadata = common::ImageMetadata(imageConfig["config"]);
}

std::tuple<common::PathRAII, common::ImageMetadata, std::string> PulledImage::expand() const {
    log(boost::format("> unpacking OCI image ..."), common::LogLevel::GENERAL);
    auto umociPath = config->json["umociPath"].GetString();
    auto umociArgs = common::CLIArguments{umociPath};

    auto umociVerbosity = utility::getUmociVerbosityOption();
    if (!umociVerbosity.empty()) {
        umociArgs.push_back(umociVerbosity);
    }

    auto expansionDir = common::PathRAII{makeTemporaryExpansionDirectory()};
    umociArgs += common::CLIArguments{"raw", "unpack", "--rootless",
                                      "--image", imageDir.getPath().string()+":sarus-pull",
                                      expansionDir.getPath().string()};

    auto start = std::chrono::system_clock::now();
    auto status = common::forkExecWait(umociArgs);
    if(status != 0) {
        auto message = boost::format("%s exited with code %d") % umociArgs % status;
        log(message, common::LogLevel::INFO);
        exit(status);
    }
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / double(1000);

    log(boost::format("Elapsed time on unpacking    : %s [sec]") % elapsed, common::LogLevel::INFO);
    log(boost::format("Successfully unpacked OCI bundle"), common::LogLevel::INFO);

    // TODO add return of configHash as imageID
    return std::tuple<common::PathRAII, common::ImageMetadata, std::string>(
        std::move(expansionDir), metadata, digest
    );
}

/**
 * Construct the orderd layer metadata from image manifest
 */
void PulledImage::initializeListOfLayersAndMetadata(web::json::value &manifest) {
    log(boost::format("initializing list of layers and metadata from image's manifest"),
        common::LogLevel::DEBUG);

    if(!manifest.has_field(U("history")) ) {
        SARUS_THROW_ERROR("manifest does not have \"history\" field.");
    }
    if(!manifest.has_field(U("fsLayers")) ) {
        SARUS_THROW_ERROR("manifest does not have \"fsLayers\" field.");
    }

    auto history = manifest.at(U("history"));

    web::json::value parent;
    web::json::value layers;
    web::json::value baseLayer;

    for(size_t idx = 0; idx < history.size(); idx++) {
        if(!history[idx].has_field(U("v1Compatibility"))) {
            SARUS_THROW_ERROR("manifest[\"history\"] does not have \"v1Compatibility\" field.");
        }

        // parse manifest[hisotry][v1Compatibility]
        std::string strJson = history[idx].at(U("v1Compatibility")).serialize();
        strJson = common::replaceString(strJson, "\\\"", "\"");
        strJson = common::replaceString(strJson, "\\\\", "\\");
        strJson = common::eraseFirstAndLastDoubleQuote(strJson);

        // TODO: web::json::value::parser() cannot work properly when escape sequences are contained.
        //       should we consider other json library?
        web::json::value layerData = web::json::value::parse(U(strJson));

        layerData[U("fsLayer")] = manifest.at(U("fsLayers"))[idx] ;

        if( layerData.has_field(U("parent")) ) {
            parent = layerData.at(U("parent"));
            if (parent.is_null()) {
                layerData[U("parent")] = web::json::value("");
                baseLayer = layerData;
            }
            else if (parent.is_string() && parent.as_string().empty()) {
                baseLayer = layerData;
            }
            else {
                layers[U(parent.serialize())] = layerData;
            }
        }
        else {
            layerData[U("parent")] = web::json::value("");
            baseLayer = layerData;
        }
    }

    // construct order list (from base to latest)
    std::vector<web::json::value> orderdLayers;
    web::json::value layer = baseLayer;
    web::json::value child;

    while(true) {
        if ( !layer.has_field(U("id")) ) {
            SARUS_THROW_ERROR("manifest[\"history\"][\"v1Compatibility\"] does not have \"id\" field.");
        }

        std::string ownId = layer.at(U("id")).serialize();

        // find child which set ownId as parent
        if ( layers.has_field(U( ownId )) )
        {
            // add current layer
            child = layers.at( ownId );
            layer[U("child")] = child.at(U("id"));
            orderdLayers.push_back(layer);
        }
        else
        {
            // add latest layer, break loop;
            layer[U("child")] = web::json::value("");
            orderdLayers.push_back(layer);
            break;
        }
        layer = child;
    }

    web::json::value lastLayer = orderdLayers[orderdLayers.size() - 1];
    auto metadataDoc = common::convertCppRestJsonToRapidJson(lastLayer);

    // get image's digest
    digest = metadataDoc["id"].GetString();
    if (!metadataDoc.HasMember("config")) {
        auto message = boost::format("Image metadata is malformed: no \"config\" field detected");
        SARUS_THROW_ERROR(message.str());
    }

    // get image's metadata
    metadata = common::ImageMetadata(metadataDoc["config"]);

    const std::string EMPTY_TAR_SHA256 = "sha256:a3ed95caeb02ffe68cdd9fd84406680ae93d633cb16422d00e8a7c22955b46d4";
    for (size_t idx = 0; idx < orderdLayers.size(); ++idx) {
        std::string digest = orderdLayers[idx].at(U("fsLayer")).at(U("blobSum")).serialize();
        digest = common::eraseFirstAndLastDoubleQuote(digest);

        if ( digest == EMPTY_TAR_SHA256 ) {
                continue; // skip if digest means empty tar
        }

        std::string filename = digest + ".tar";
        this->layers.push_back(config->directories.cache / filename);
    }

    log(boost::format("successfully initialized list of layers and metadata from image's manifest"),
        common::LogLevel::DEBUG);
}

}
}
