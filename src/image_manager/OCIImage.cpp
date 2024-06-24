/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "OCIImage.hpp"

#include "libsarus/PathRAII.hpp"
#include "libsarus/Utility.hpp"
#include "image_manager/Utility.hpp"
#include "image_manager/UmociDriver.hpp"


namespace sarus {
namespace image_manager {

OCIImage::OCIImage(std::shared_ptr<const common::Config> config, const boost::filesystem::path& imagePath)
    : config{std::move(config)},
      imageDir{imagePath}
{
    log(boost::format("Creating OCIImage object from image at %s") % imageDir.getPath(), libsarus::LogLevel::DEBUG);
    auto imageIndex = libsarus::readJSON(imageDir.getPath() / "index.json");
    auto schemaItr = imageIndex.FindMember("schemaVersion");
    if (schemaItr == imageIndex.MemberEnd() || schemaItr->value.GetUint() != 2) {
        SARUS_THROW_ERROR("Unsupported OCI image index format. The 'schemaVersion' property could not be found or its value is different from '2'");
    }

    std::string manifestDigest = imageIndex["manifests"][0]["digest"].GetString();
    log(boost::format("Found manifest digest: %s") % manifestDigest, libsarus::LogLevel::DEBUG);
    auto manifestHash = manifestDigest.substr(manifestDigest.find(":")+1);
    auto imageManifest = libsarus::readJSON(imageDir.getPath() / "blobs/sha256" / manifestHash);

    std::string configDigest = imageManifest["config"]["digest"].GetString();
    log(boost::format("Found config digest: %s") % configDigest, libsarus::LogLevel::DEBUG);
    auto configHash = configDigest.substr(configDigest.find(":")+1);
    auto imageConfig = libsarus::readJSON(imageDir.getPath() / "blobs/sha256" / configHash);

    metadata = sarus::common::ImageMetadata(imageConfig["config"]);
    imageID = configHash;
}

libsarus::PathRAII OCIImage::unpack() const {
    log(boost::format("> unpacking OCI image"), libsarus::LogLevel::GENERAL);

    auto unpackDir = libsarus::PathRAII{makeTemporaryUnpackDirectory()};

    auto umociDriver = UmociDriver{config};
    umociDriver.unpack(imageDir.getPath(), unpackDir.getPath());

    log(boost::format("Successfully unpacked OCI image"), libsarus::LogLevel::INFO);
    return unpackDir;
}

boost::filesystem::path OCIImage::makeTemporaryUnpackDirectory() const {
    auto tempUnpackDir = libsarus::makeUniquePathWithRandomSuffix(config->directories.temp / "unpack-directory");
    try {
        libsarus::createFoldersIfNecessary(tempUnpackDir);
    }
    catch(libsarus::Error& e) {
        auto message = boost::format("Error creating temporary unpacking directory %s") % tempUnpackDir;
        SARUS_RETHROW_ERROR(e, message.str());
    }
    return tempUnpackDir;
}

void OCIImage::release() {
    imageDir.release();
}

void OCIImage::log(const boost::format &message, libsarus::LogLevel level,
                   std::ostream& outStream, std::ostream& errStream) const {
    log(message.str(), level, outStream, errStream);
}

void OCIImage::log(const std::string& message, libsarus::LogLevel level,
                   std::ostream& outStream, std::ostream& errStream) const {
    libsarus::Logger::getInstance().log(message, "OCIImage", level, outStream, errStream);
}

}} // namespace
