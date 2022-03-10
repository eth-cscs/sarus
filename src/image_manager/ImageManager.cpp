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

#include <iostream>
#include <chrono> // for timer

#include <cpprest/json.h>
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/PathRAII.hpp"
#include "common/Utility.hpp"
#include "image_manager/LoadedImage.hpp"
#include "image_manager/SquashfsImage.hpp"


namespace sarus {
namespace image_manager {

    ImageManager::ImageManager(std::shared_ptr<const common::Config> config)
    : config(config)
    , puller(config)
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

        auto pulledImage = puller.pull();
        processImage(pulledImage);

        printLog(boost::format("Successfully pulled image"), common::LogLevel::INFO);
    }

    /**
     * Load the container archive image, and add to the repository
     */
    void ImageManager::loadImage(const boost::filesystem::path& archive) {
        issueErrorIfIsCentralizedRepositoryAndCentralizedRepositoryIsDisabled();
        issueWarningIfIsCentralizedRepositoryAndIsNotRootUser();

        printLog(boost::format("Loading image archive %s") % archive, common::LogLevel::INFO);

        auto loadedImage = LoadedImage{config, archive};
        processImage(loadedImage);

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

    void ImageManager::processImage(const InputImage& image) {
        common::PathRAII expandedImage;
        common::ImageMetadata metadata;
        std::string digest;
        std::tie(expandedImage, metadata, digest) = image.expand();

        metadata.write(config->getMetadataFileOfImage());
        auto metadataRAII = common::PathRAII{config->getMetadataFileOfImage()};

        auto squashfs = SquashfsImage{*config, expandedImage.getPath(), config->getImageFile()};
        auto squashfsRAII = common::PathRAII{squashfs.getPathOfImage()};

        auto imageSize = common::getFileSize(config->getImageFile());
        auto imageSizeString = common::SarusImage::createSizeString(imageSize);
        auto created = common::SarusImage::createTimeString(std::time(nullptr));
        // TODO Add ImageID to SarusImage (corresponds to OCI image config hash to be returned by image.expand())
        auto sarusImage = common::SarusImage{
            config->imageReference,
            digest,
            imageSizeString,
            created,
            squashfsRAII.getPath(),
            metadataRAII.getPath()};

        imageStore.addImage(sarusImage);

        metadataRAII.release();
        squashfsRAII.release();
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
        common::Logger::getInstance().log(message.str(), sysname, LogLevel, outStream, errStream);
    }

} // namespace
} // namespace
