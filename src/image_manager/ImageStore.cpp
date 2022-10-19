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

#include <boost/algorithm/string.hpp>
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
    void ImageStore::addImage(const common::SarusImage& image) const {
        printLog(boost::format("Adding image %s to metadata file %s")
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

        atomicallyUpdateRepositoryMetadataFile(metadata);

        printLog(boost::format("Successfully added image"), common::LogLevel::INFO);
    }

    /**
     * Remove container image from repository
     */
    void ImageStore::removeImage(const common::ImageReference& imageReference) const {
        printLog(boost::format("Attempting to remove image %s from local repository") % imageReference,
                 common::LogLevel::INFO);
        common::Lockfile lock{metadataFile};

        auto repositoryMetadata = readRepositoryMetadata();
        auto imageMetadata = findImageMetadata(imageReference, repositoryMetadata);

        if (!imageMetadata) {
            auto message = boost::format("Cannot find image '%s'") % imageReference;
            printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }

        try {
            // Attempting to remove backing files first so that, if something goes wrong on metadata removal,
            // the orphaned metadata have more chance to be cleaned during a subsequent "sarus images" or "sarus run" command.
            // If we remove metadata first and something goes wrong on backing files removal
            // there would be no data-driven way to reach the orphaned files, which would just lie in the filesystem occupying space.
            removeImageBackingFiles(imageMetadata);
            removeRepositoryMetadataEntry(imageMetadata, repositoryMetadata);
        }
        catch(std::exception& e) {
            auto message = boost::format("Failed to remove image %s") % imageReference;
            SARUS_RETHROW_ERROR(e, message.str());
        }

        printLog(boost::format("Successfully removed image from local repository"), common::LogLevel::INFO);
    }

    /**
     * List the containers in repository
     */
    std::vector<common::SarusImage> ImageStore::listImages() const {
        common::Lockfile lock{metadataFile};
        auto repositoryMetadata = readRepositoryMetadata();
        auto images = std::vector<common::SarusImage>{};

        for (const auto& imageMetadata : repositoryMetadata["images"].GetArray()) {
            // If backing files are present, all image data is available: add the image to list to be visualized.
            // Else, ensure all image data is cleaned up
            if (hasImageBackingFiles(imageMetadata)) {
                auto image = convertImageMetadataToSarusImage(imageMetadata);
                images.push_back(image);
            }
            else {
                removeImageBackingFiles(&imageMetadata);
                removeRepositoryMetadataEntry(&imageMetadata, repositoryMetadata);
            }
        }

        printLog(boost::format("Successfully created list of images."), common::LogLevel::DEBUG);
        return images;
    }

    boost::optional<common::SarusImage> ImageStore::findImage(const common::ImageReference& reference) const {
        printLog(boost::format("Looking for reference '%s' in local repository") % reference, common::LogLevel::DEBUG);
        common::Lockfile lock{metadataFile};
        boost::optional<common::SarusImage> image;

        auto repositoryMetadata = readRepositoryMetadata();
        auto imageMetadata = findImageMetadata(reference, repositoryMetadata);

        if (imageMetadata) {
            // If backing files are present, all image data is available: assign object to return
            // Else, ensure all image data is cleaned up
            if (hasImageBackingFiles(*imageMetadata)) {
                image = convertImageMetadataToSarusImage(*imageMetadata);
            }
            else {
                removeImageBackingFiles(imageMetadata);
                repositoryMetadata["images"].GetArray().Erase(imageMetadata);
            }
        }

        // Update repository metadata in case entry was removed due to missing backing files
        atomicallyUpdateRepositoryMetadataFile(repositoryMetadata);

        printLog(boost::format("Image for reference '%s' %s") % reference % (image ? "found" : "not found"),
                 common::LogLevel::DEBUG);
        return image;
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

    bool ImageStore::hasImageBackingFiles(const rapidjson::Value& imageMetadata) const {
        auto imagePath = boost::filesystem::path{(imageMetadata)["imagePath"].GetString()};
        auto metadataPath = boost::filesystem::path{(imageMetadata)["metadataPath"].GetString()};
        auto missing = std::vector<std::string>{};
        if(!boost::filesystem::exists(imagePath)) {
            missing.push_back(imagePath.string());
        }
        if(!boost::filesystem::exists(metadataPath)) {
            missing.push_back(metadataPath.string());
        }
        if(!missing.empty()) {
            auto message = boost::format("Repository inconsistency detected: image is listed in the repository "
                                         "metadata but the following backing files are missing: %s")
                                         % boost::algorithm::join(missing, ", ");
            printLog(message.str(), common::LogLevel::INFO, std::cerr);
        }
        return missing.empty();
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
            // for forward compatibility of Sarus 1.4.2 and earlier versions
            ret.AddMember(  "digest",
                            rj::Value{"", allocator},
                            allocator);
            ret.AddMember(  "registryDigest",
                            rj::Value{image.reference.digest.c_str(), allocator},
                            allocator);
            ret.AddMember(  "id",
                            rj::Value{image.id.c_str(), allocator},
                            allocator);
            ret.AddMember(  "imagePath",
                            rj::Value{image.imageFile.c_str(), allocator},
                            allocator);
            ret.AddMember(  "metadataPath",
                            rj::Value{image.metadataFile.c_str(), allocator},
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
     * Deletes an image entry from the repository's overall metadata.json
     * IMPORTANT: this function does not lock the metadata file on its own!
     *            Use this function from a caller performing the lock!
     */
    void ImageStore::removeRepositoryMetadataEntry(const rapidjson::Value* imageEntry, rapidjson::Document& repositoryMetadata) const {
        repositoryMetadata["images"].GetArray().Erase(imageEntry);
        atomicallyUpdateRepositoryMetadataFile(repositoryMetadata);
        printLog("Removed image entry from repository metadata", common::LogLevel::DEBUG);
    }

    /**
     * Deletes the image's individual squashfs file and metadata file
     */
    void ImageStore::removeImageBackingFiles(const rapidjson::Value* imageMetadata) const {
        auto imagePath = boost::filesystem::path{(*imageMetadata)["imagePath"].GetString()};
        auto metadataPath = boost::filesystem::path{(*imageMetadata)["metadataPath"].GetString()};
        boost::filesystem::remove_all(imagePath);
        boost::filesystem::remove_all(metadataPath);
        printLog("Removed image backing files", common::LogLevel::DEBUG);
    }

    /**
     * Atomically update the repository's metadata file. Creates a temporary metadata file
     * and then atomically creates/replaces the actual metadata file by renaming the
     * temporary one.
     */
    void ImageStore::atomicallyUpdateRepositoryMetadataFile(const rapidjson::Value& metadata) const {
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

    boost::filesystem::path ImageStore::getImageSquashfsFile(const common::ImageReference& reference) const {
        auto relativePath = reference.getUniqueKey() + ".squashfs";
        return imagesDirectory / relativePath;
    }

    boost::filesystem::path ImageStore::getImageMetadataFile(const common::ImageReference& reference) const {
        auto relativePath = reference.getUniqueKey() + ".meta";
        return imagesDirectory / relativePath;
    }

    void ImageStore::printLog(const boost::format& message, common::LogLevel LogLevel,
                              std::ostream& out, std::ostream& err) const {
        printLog(message.str(), LogLevel, out, err);
    }

    void ImageStore::printLog(const std::string& message, common::LogLevel LogLevel,
                              std::ostream& out, std::ostream& err) const {
        common::Logger::getInstance().log(message, sysname, LogLevel, out, err);
    }

} // namespace
} // namespace
