/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "OCIImage.hpp"

#include "common/PathRAII.hpp"
#include "common/Utility.hpp"
#include "image_manager/Utility.hpp"


namespace sarus {
namespace image_manager {

OCIImage::OCIImage(std::shared_ptr<const common::Config> config, const boost::filesystem::path& imagePath)
    : config{std::move(config)},
      imageDir{imagePath}
{
    auto imageIndex = common::readJSON(imageDir.getPath() / "index.json");
    auto schemaItr = imageIndex.FindMember("schemaVersion");
    if (schemaItr == imageIndex.MemberEnd() || schemaItr->value.GetUint() != 2) {
        SARUS_THROW_ERROR("Unsupported OCI image index format. The 'schemaVersion' property could not be found or its value is different from '2'");
    }

    std::string manifestDigest = imageIndex["manifests"][0]["digest"].GetString();
    auto manifestHash = manifestDigest.substr(manifestDigest.find(":")+1);
    auto imageManifest = common::readJSON(imageDir.getPath() / "blobs/sha256" / manifestHash);

    std::string configDigest = imageManifest["config"]["digest"].GetString();
    auto configHash = configDigest.substr(configDigest.find(":")+1);
    auto imageConfig = common::readJSON(imageDir.getPath() / "blobs/sha256" / configHash);

    metadata = common::ImageMetadata(imageConfig["config"]);
    imageID = configHash;
}

common::PathRAII OCIImage::expand() const {
    log(boost::format("> unpacking OCI image ..."), common::LogLevel::GENERAL);
    auto umociPath = config->json["umociPath"].GetString();
    auto umociArgs = common::CLIArguments{umociPath};

    auto umociVerbosity = utility::getUmociVerbosityOption();
    if (!umociVerbosity.empty()) {
        umociArgs.push_back(umociVerbosity);
    }

    auto expansionDir = common::PathRAII{makeTemporaryExpansionDirectory()};
    umociArgs += common::CLIArguments{"raw", "unpack", "--rootless",
                                      "--image", imageDir.getPath().string()+":sarus-oci-image",
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
    log(boost::format("Successfully unpacked OCI image"), common::LogLevel::INFO);

    return std::move(expansionDir);
}

boost::filesystem::path OCIImage::makeTemporaryExpansionDirectory() const {
    auto tempExpansionDir = common::makeUniquePathWithRandomSuffix(config->directories.temp / "expansion-directory");
    try {
        common::createFoldersIfNecessary(tempExpansionDir);
    }
    catch(common::Error& e) {
        auto message = boost::format("Error creating temporary expansion directory %s") % config->directories.temp;
        SARUS_RETHROW_ERROR(e, message.str());
    }
    return tempExpansionDir;
}

void OCIImage::release() {
    imageDir.release();
}

void OCIImage::log(const boost::format &message, common::LogLevel level,
                   std::ostream& outStream, std::ostream& errStream) const {
    log(message.str(), level, outStream, errStream);
}

void OCIImage::log(const std::string& message, common::LogLevel level,
                   std::ostream& outStream, std::ostream& errStream) const {
    common::Logger::getInstance().log(message, "OCIImage", level, outStream, errStream);
}

}} // namespace
