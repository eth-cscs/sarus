/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "image_manager/ImageStore.hpp"

#include <vector>
#include <iostream>
#include <string>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "common/PathRAII.hpp"
#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "common/Utility.hpp"
#include "common/Lockfile.hpp"
#include "common/SarusImage.hpp"


namespace rj = rapidjson;

namespace sarus {
namespace image_manager {

    ImageStore::ImageStore(std::shared_ptr<const common::Config> config)
        : imagesDirectory{config->directories.images}
        , metadataFile{config->directories.repository / "metadata.json"}
    {}

    /**
     * Add the container image into repository (or update existing object)
     */
    void ImageStore::addImage(const common::SarusImage& image) {
        printLog(   boost::format("Adding image %s to metadata file %s")
                    % image.reference % metadataFile,
                    common::LogLevel::INFO);

        common::Lockfile lock{metadataFile};
        auto metadata = readRepositoryMetadata();

        // remove previous entries with the same image reference (if any)
        auto& images = metadata["images"];
        for(auto it = images.Begin(); it != images.End(); ) {
            if ((*it)["uniqueKey"].GetString() == image.reference.getUniqueKey()) {
                it = images.Erase(it);
            } else {
                ++it;
            }
        }

        // add new metadata entry
        metadata["images"].GetArray().PushBack(
            createImageJSON(image, metadata.GetAllocator()),
            metadata.GetAllocator());

        atomicallyUpdateMetadataFile(metadata);

        printLog(boost::format("Successfully added image"), common::LogLevel::INFO);
    }

    /**
     * Remove container image from repository
     */
    void ImageStore::removeImage(const common::ImageReference& imageReference) {
        printLog(   boost::format("Removing image %s from metadata file %s")
                    % imageReference % metadataFile,
                    common::LogLevel::INFO);
        common::Lockfile lock{metadataFile};

        auto repositoryMetadata = readRepositoryMetadata();
        auto imageMetadata = findImageMetadata(imageReference, repositoryMetadata);

        if (!imageMetadata) {
            auto message = boost::format("Cannot find image '%s'") % imageReference;
            printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }

        // remove image
        try {
            auto imagePath = boost::filesystem::path{(*imageMetadata)["imagePath"].GetString()};
            auto metadataPath = boost::filesystem::path{(*imageMetadata)["metadataPath"].GetString()};

            repositoryMetadata["images"].GetArray().Erase(imageMetadata);
            atomicallyUpdateMetadataFile(repositoryMetadata);

            boost::filesystem::remove_all(imagePath);
            boost::filesystem::remove_all(metadataPath);
        }
        catch(std::exception& e) {
            auto message = boost::format("Failed to remove image %s") % imageReference;
            SARUS_RETHROW_ERROR(e, message.str());
        }

        printLog(boost::format("Successfully removed image"), common::LogLevel::INFO);
    }

    /**
     * List the containers in repository
     */
    std::vector<common::SarusImage> ImageStore::listImages() const
    {
        auto metadata = readRepositoryMetadata();
        auto images = std::vector<common::SarusImage>{};

        for (const auto& imageMetadata : metadata["images"].GetArray()) {
            auto image = convertImageMetadataToSarusImage(imageMetadata);
            images.push_back(image);
        }

        printLog(boost::format("Successfully created list of images."), common::LogLevel::DEBUG);
        return images;
    }

    /**
     * The "id" property was introduced with Sarus 1.5.0
     * This function provides compatibility with image metadata created by an earlier Sarus version
     */
    std::string ImageStore::getImageID(const rapidjson::Value& imageMetadata) const {
        auto itr = imageMetadata.FindMember("id");
        if (itr != imageMetadata.MemberEnd()) {
            return itr->value.GetString();
        }
        return std::string{};
    }

    /**
     * The "registryDigest" property was introduced with Sarus 1.5.0
     * This function provides compatibility with image metadata created by an earlier Sarus version
     */
    std::string ImageStore::getRegistryDigest(const rapidjson::Value& imageMetadata) const {
        auto itr = imageMetadata.FindMember("registryDigest");
        if (itr != imageMetadata.MemberEnd()) {
            return itr->value.GetString();
        }
        return std::string{};
    }

    rapidjson::Document ImageStore::readRepositoryMetadata() const {
        rj::Document metadata;

        // if metadata already exists, load it
        if (boost::filesystem::exists(metadataFile)) {
            printLog( boost::format("metadata already exists. Try to read json.") , common::LogLevel::DEBUG);
            metadata = common::readJSON(metadataFile);
        }
        // otherwise create new metadata with empty images array
        else {
            metadata = rj::Document{rj::kObjectType};
            metadata.AddMember("images", rj::kArrayType, metadata.GetAllocator());
        }

        return metadata;
    }

    boost::optional<common::SarusImage> ImageStore::findImage(const common::ImageReference& reference) const {
        printLog(boost::format("Looking for reference '%s' in storage") % reference, common::LogLevel::DEBUG);
        boost::optional<common::SarusImage> image;

        auto repositoryMetadata = readRepositoryMetadata();
        auto imageMetadata = findImageMetadata(reference, repositoryMetadata);

        if (imageMetadata) {
            image = convertImageMetadataToSarusImage(*imageMetadata);
        }

        printLog(boost::format("Image for reference '%s' %s") % reference % (image ? "found" : "not found"),
                 common::LogLevel::DEBUG);
        return image;
    }

    const rapidjson::Value* ImageStore::findImageMetadata(const common::ImageReference& reference, const rapidjson::Document& metadata) const {
        printLog(boost::format("Looking for reference '%s' in repository metadata") % reference, common::LogLevel::DEBUG);
        const rj::Value* imageMetadata = nullptr;
        auto uniqueKey = reference.getUniqueKey();

        for(const auto& image : metadata["images"].GetArray()) {
            if(image["uniqueKey"].GetString() == uniqueKey) {
                imageMetadata = &image;
                break;
            }
        }

        printLog(boost::format("Metadata for reference '%s' %s") % reference % (imageMetadata ? "found" : "not found"),
                 common::LogLevel::DEBUG);
        return imageMetadata;
    }

    common::SarusImage ImageStore::convertImageMetadataToSarusImage(const rapidjson::Value& imageMetadata) const {
        auto imageReference = common::ImageReference{
            imageMetadata["server"].GetString(),
            imageMetadata["namespace"].GetString(),
            imageMetadata["image"].GetString(),
            imageMetadata["tag"].GetString(),
            getRegistryDigest(imageMetadata)};
        auto image = common::SarusImage{
            imageReference,
            getImageID(imageMetadata),
            imageMetadata["datasize"].GetString(),
            imageMetadata["created"].GetString(),
            boost::filesystem::path{imageMetadata["imagePath"].GetString()},
            boost::filesystem::path{imageMetadata["metadataPath"].GetString()}
        };
        return image;
    }

    rapidjson::Value ImageStore::createImageJSON(const common::SarusImage& image, rapidjson::MemoryPoolAllocator<>& allocator) const {
        try {
            auto ret = rj::Value{rj::kObjectType};
            ret.AddMember(  "uniqueKey",
                            rj::Value{image.reference.getUniqueKey().c_str(), allocator},
                            allocator);
            ret.AddMember(  "server",
                            rj::Value{image.reference.server.c_str(), allocator},
                            allocator);
            ret.AddMember(  "namespace",
                            rj::Value{image.reference.repositoryNamespace.c_str(), allocator},
                            allocator);
            ret.AddMember(  "image",
                            rj::Value{image.reference.image.c_str(), allocator},
                            allocator);
            ret.AddMember(  "tag",
                            rj::Value{image.reference.tag.c_str(), allocator},
                            allocator);
            ret.AddMember(  "registryDigest",
                            rj::Value{image.reference.digest.c_str(), allocator},
                            allocator);
            ret.AddMember(  "id",
                            rj::Value{image.id.c_str(), allocator},
                            allocator);
            ret.AddMember(  "imagePath",
                            rj::Value{image.imageFile.string().c_str(), allocator},
                            allocator);
            ret.AddMember(  "metadataPath",
                            rj::Value{image.metadataFile.string().c_str(), allocator},
                            allocator);
            ret.AddMember(  "datasize",
                            rj::Value{image.datasize.c_str(), allocator},
                            allocator);
            ret.AddMember(  "created",
                            rj::Value{image.created.c_str(), allocator},
                            allocator);
            return ret;
        }
        catch (const std::exception &e) {
            SARUS_RETHROW_ERROR(e, "Failed to create JSON of image object.");
        }
    }

    /**
     * Atomically update the repository's metadata file. Creates a temporary metadata file
     * and then atomically creates/replaces the actual metadata file by renaming the
     * temporary one.
     */
    void ImageStore::atomicallyUpdateMetadataFile(const rapidjson::Value& metadata) const {
        auto metadataFileTemp = common::makeUniquePathWithRandomSuffix(metadataFile);

        printLog( boost::format("Update metadata: %s") % metadataFile, common::LogLevel::DEBUG);

        try {
            common::writeJSON(metadata, metadataFileTemp);
            boost::filesystem::rename(metadataFileTemp, metadataFile); // atomically replace old metadata file
        }
        catch (const std::exception &e) {
            auto message = boost::format("Failed to write metadata file %s") % metadataFile;
            SARUS_RETHROW_ERROR(e, message.str());
        }

        printLog( boost::format("Success to update metadata: %s") % metadataFile, common::LogLevel::DEBUG);
    }

    boost::filesystem::path ImageStore::getImageFile(const common::ImageReference& reference) const {
        auto relativePath = reference.getUniqueKey() + ".squashfs";
        return imagesDirectory / relativePath;
    }

    boost::filesystem::path ImageStore::getMetadataFileOfImage(const common::ImageReference& reference) const {
        auto relativePath = reference.getUniqueKey() + ".meta";
        return imagesDirectory / relativePath;
    }

    void ImageStore::printLog(  const boost::format &message, common::LogLevel LogLevel,
                                std::ostream& out, std::ostream& err) const {
        common::Logger::getInstance().log( message.str(), sysname, LogLevel, out, err);
    }

} // namespace
} // namespace
