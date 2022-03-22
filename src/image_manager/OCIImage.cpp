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
#include "image_manager/UmociDriver.hpp"


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

common::PathRAII OCIImage::unpack() const {
    log(boost::format("> unpacking OCI image"), common::LogLevel::GENERAL);

    auto unpackDir = common::PathRAII{makeTemporaryUnpackDirectory()};

    auto umociDriver = UmociDriver{config};
    umociDriver.unpack(imageDir.getPath(), unpackDir.getPath());

    log(boost::format("Successfully unpacked OCI image"), common::LogLevel::INFO);
    return std::move(unpackDir);
}

boost::filesystem::path OCIImage::makeTemporaryUnpackDirectory() const {
    auto tempUnpackDir = common::makeUniquePathWithRandomSuffix(config->directories.temp / "unpack-directory");
    try {
        common::createFoldersIfNecessary(tempUnpackDir);
    }
    catch(common::Error& e) {
        auto message = boost::format("Error creating temporary unpacking directory %s") % tempUnpackDir;
        SARUS_RETHROW_ERROR(e, message.str());
    }
    return tempUnpackDir;
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
