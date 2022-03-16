/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "LoadedImage.hpp"

#include "common/PathRAII.hpp"
#include "common/Utility.hpp"
#include "image_manager/Utility.hpp"


namespace sarus {
namespace image_manager {

LoadedImage::LoadedImage(std::shared_ptr<const common::Config> config, const boost::filesystem::path& imagePath)
    : InputImage{std::move(config)},
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

std::tuple<common::PathRAII, common::ImageMetadata, std::string> LoadedImage::expand() const {
    log(boost::format("> unpacking OCI image ..."), common::LogLevel::GENERAL);
    auto umociPath = config->json["umociPath"].GetString();
    auto umociArgs = common::CLIArguments{umociPath};

    auto umociVerbosity = utility::getUmociVerbosityOption();
    if (!umociVerbosity.empty()) {
        umociArgs.push_back(umociVerbosity);
    }

    auto expansionDir = common::PathRAII{makeTemporaryExpansionDirectory()};
    umociArgs += common::CLIArguments{"raw", "unpack", "--rootless",
                                      "--image", imageDir.getPath().string()+":sarus-load",
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
        std::move(expansionDir), metadata, std::string{}
    );
}

}} // namespace
