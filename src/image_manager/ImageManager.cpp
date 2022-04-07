/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "image_manager/ImageManager.hpp"

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/PathRAII.hpp"
#include "common/Utility.hpp"
#include "image_manager/SquashfsImage.hpp"
#include "image_manager/Utility.hpp"


namespace sarus {
namespace image_manager {

    ImageManager::ImageManager(std::shared_ptr<const common::Config> config)
    : config(config)
    , skopeoDriver(config)
    , imageStore(config)
    {}

    /**
     * Pull the container image and add to the repository
     */
    void ImageManager::pullImage() {
        issueErrorIfPullingByDigest();
        issueErrorIfIsCentralizedRepositoryAndCentralizedRepositoryIsDisabled();
        issueWarningIfIsCentralizedRepositoryAndIsNotRootUser();

        printLog(boost::format("Pulling image %s") % config->imageReference, common::LogLevel::INFO);

        // output params
        printLog( boost::format("# image            : %s") % config->imageReference, common::LogLevel::GENERAL);
        printLog( boost::format("# cache directory  : %s") % config->directories.cache, common::LogLevel::GENERAL);
        printLog( boost::format("# temp directory   : %s") % config->directories.temp, common::LogLevel::GENERAL);
        printLog( boost::format("# images directory : %s") % config->directories.images, common::LogLevel::GENERAL);

        std::string imageDigest = retrieveImageDigest();
        printLog( boost::format("# image digest     : %s") % imageDigest, common::LogLevel::GENERAL);

        auto ociImagePath = skopeoDriver.copyToOCIImage("docker", config->imageReference.string());
        processImage(OCIImage{config, ociImagePath}, imageDigest);

        printLog(boost::format("Successfully pulled image"), common::LogLevel::INFO);
    }

    /**
     * Load the container archive image, and add to the repository
     */
    void ImageManager::loadImage(const boost::filesystem::path& archive) {
        issueErrorIfIsCentralizedRepositoryAndCentralizedRepositoryIsDisabled();
        issueWarningIfIsCentralizedRepositoryAndIsNotRootUser();

        printLog(boost::format("Loading image archive %s") % archive, common::LogLevel::INFO);

        auto ociImagePath = skopeoDriver.copyToOCIImage("docker-archive", archive.string());
        processImage(OCIImage{config, ociImagePath}, std::string{});

        printLog(boost::format("Successfully loaded image archive"), common::LogLevel::INFO);
    }

    /**
     * Show the list of available images in repository
     */
    std::vector<common::SarusImage> ImageManager::listImages() const {
        return imageStore.listImages();
    }

    /**
     * Remove the image data from repository
     */
    void ImageManager::removeImage() {
        issueErrorIfIsCentralizedRepositoryAndCentralizedRepositoryIsDisabled();
        issueWarningIfIsCentralizedRepositoryAndIsNotRootUser();

        printLog(boost::format("removing image %s") % config->imageReference, common::LogLevel::INFO);

        imageStore.removeImage(config->imageReference);

        printLog(boost::format("removed image %s") % config->imageReference, common::LogLevel::GENERAL);
        printLog(boost::format("successfully removed image"), common::LogLevel::INFO);
    }

    void ImageManager::processImage(const OCIImage& image, const std::string& digest) {
        auto metadata = image.getMetadata();
        metadata.write(config->getMetadataFileOfImage());
        auto metadataRAII = common::PathRAII{config->getMetadataFileOfImage()};

        auto unpackedImage = image.unpack();
        auto squashfs = SquashfsImage{*config, unpackedImage.getPath(), config->getImageFile()};
        auto squashfsRAII = common::PathRAII{squashfs.getPathOfImage()};

        auto imageSize = common::getFileSize(config->getImageFile());
        auto imageSizeString = common::SarusImage::createSizeString(imageSize);
        auto created = common::SarusImage::createTimeString(std::time(nullptr));
        auto sarusImage = common::SarusImage{
            config->imageReference,
            image.getImageID(),
            digest,
            imageSizeString,
            created,
            squashfsRAII.getPath(),
            metadataRAII.getPath()};

        imageStore.addImage(sarusImage);

        metadataRAII.release();
        squashfsRAII.release();
    }

    std::string ImageManager::retrieveImageDigest() const {
        auto imageDigest = std::string{};
        auto inspectOutput = skopeoDriver.inspectRaw("docker", config->imageReference.string());

        auto mediaTypeItr =  inspectOutput.FindMember("mediaType");
        if (mediaTypeItr == inspectOutput.MemberEnd()) {
            auto message = boost::format("Failed to pull image '%s'\n"
                                         "Unknown manifest media type returned by remote registry."
                                         "The 'mediaType' property could not be found") % config->imageReference;
            SARUS_THROW_ERROR(message.str());
        }

        auto mediaType = std::string{mediaTypeItr->value.GetString()};
        // If we have a recognized manifest type, the image digest is the sha256 digest of the manifest
        if (mediaType == "application/vnd.oci.image.manifest.v1+json"
            || mediaType == "application/vnd.docker.distribution.manifest.v2+json"
            || mediaType == "application/vnd.docker.distribution.manifest.v1+json") {
            printLog("Computing image digest from raw manifest", common::LogLevel::INFO);
            auto manifestFile = common::PathRAII(common::makeUniquePathWithRandomSuffix(config->directories.temp / "sarusPullManifest"));
            common::writeJSON(inspectOutput, manifestFile.getPath());
            imageDigest = skopeoDriver.manifestDigest(manifestFile.getPath());
        }
        // If we have an OCI index or Docker manifest list (aka "fat manifest"), retrieve the digest
        // of the manifest for the current platform (hardware arch + OS)
        else if (mediaType == "application/vnd.oci.image.index.v1+json"
                 || mediaType == "application/vnd.docker.distribution.manifest.list.v2+json") {
            printLog("Retrieving image digest from OCI index or Docker manifest list", common::LogLevel::INFO);
            auto platform = utility::getCurrentOCIPlatform();
            imageDigest = utility::getPlatformDigestFromOCIIndex(inspectOutput, platform);
            if (imageDigest.empty()) {
                printLog("Unable to retrieve registry digest for image being pulled. Attempting to continue with empty digest",
                         common::LogLevel::WARN);
            }
        }
        // The OCI Image spec states that mediaTypes unknown to the implementation must be ignored
        else {
            auto message = boost::format("Unknown mediaType of manifest returned by remote registry: %s. "
                                         "Attempting to continue with empty digest") % mediaType;
            printLog(message, common::LogLevel::WARN);
        }

        auto message = boost::format("Got image digest: %s") % imageDigest;
        printLog(message, common::LogLevel::INFO);

        return imageDigest;
    }

    void ImageManager::issueErrorIfIsCentralizedRepositoryAndCentralizedRepositoryIsDisabled() const {
        if(config->useCentralizedRepository && !common::isCentralizedRepositoryEnabled(*config)) {
            SARUS_THROW_ERROR("attempting to perform an operation on the centralized repository,"
                                " but the centralized repository is disabled. Please contact your system"
                                " administrator to configure the centralized repository.");
        }
    }

    void ImageManager::issueWarningIfIsCentralizedRepositoryAndIsNotRootUser() const {
        bool isRoot = config->userIdentity.uid == 0;
        if(config->useCentralizedRepository && !isRoot) {
            auto message = boost::format("attempting to perform an operation on the"
                " centralized repository without root privileges");
            printLog(message, common::LogLevel::WARN);
        }
    }

    void ImageManager::issueErrorIfPullingByDigest() const {
        // Build regexp for catching digests with "@" + first part of the digest regexp (before colon separator)
        // from https://github.com/opencontainers/go-digest/blob/master/digest.go
        boost::regex digestRegexp("@[a-z0-9]+(?:[.+_-][a-z0-9]+)*");
        boost::smatch matches;
        if(boost::regex_search(config->imageReference.image, matches, digestRegexp, boost::regex_constants::match_partial)) {
            auto message = boost::format("Pulling images by digest is currently not supported. "
                                         "The feature will be introduced in a future release");
            printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }
    }

    void ImageManager::printLog(const boost::format& message, common::LogLevel LogLevel, std::ostream& outStream, std::ostream& errStream) const {
        printLog(message.str(), LogLevel, outStream, errStream);
    }

    void ImageManager::printLog(const std::string& message, common::LogLevel LogLevel, std::ostream& outStream, std::ostream& errStream) const {
        common::Logger::getInstance().log(message, sysname, LogLevel, outStream, errStream);
    }

} // namespace
} // namespace
