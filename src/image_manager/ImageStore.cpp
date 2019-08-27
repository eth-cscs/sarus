/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
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

#include <boost/optional/optional.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "common/Utility.hpp"
#include "common/Lockfile.hpp"
#include "common/SarusImage.hpp"


namespace rj = rapidjson;

namespace sarus {
namespace image_manager {

    ImageStore::ImageStore(std::shared_ptr<const common::Config> config)
        : config{config}
        , metadataFile{config->directories.repository / "metadata.json"}
    {}

    /**
     * Add the container image into repository (or update existing object)
     */
    void ImageStore::addImage(const common::SarusImage& image) {
        printLog(   boost::format("Adding image %s to metadata file %s")
                    % image.imageID % metadataFile,
                    common::LogLevel::INFO);

        common::Lockfile lock{metadataFile};
        auto metadata = readRepositoryMetadata();

        // remove previous entries with the same image ID (if any)
        auto& images = metadata["images"];
        for(auto it = images.Begin(); it != images.End(); ) {
            if ((*it)["uniqueKey"].GetString() == image.imageID.getUniqueKey()) {
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
    void ImageStore::removeImage(const common::ImageID& imageID) {
        printLog(   boost::format("Removing image %s from metadata file %s")
                    % imageID % metadataFile,
                    common::LogLevel::INFO);

        common::Lockfile lock{metadataFile};

        auto metadata = readRepositoryMetadata();
        const rj::Value* imageMetadata = nullptr;
        auto uniqueKey = imageID.getUniqueKey();

        printLog(boost::format("looking up unique key %s in metadata file") % uniqueKey, common::LogLevel::DEBUG);
        
        // look for image's metadata
        for(const auto& image : metadata["images"].GetArray()) {
            if(image["uniqueKey"].GetString() == uniqueKey) {
                imageMetadata = &image;
                break;
            }
        }

        if (!imageMetadata) {
            auto message = boost::format("failed to find unique key %s in metadata file %s")
                % uniqueKey % metadataFile;
            SARUS_THROW_ERROR(message.str());
        }

        printLog(boost::format("Success to find unique key"), common::LogLevel::DEBUG);

        // remove image
        try {
            auto imagePath = boost::filesystem::path{(*imageMetadata)["imagePath"].GetString()};
            auto metadataPath = boost::filesystem::path{(*imageMetadata)["metadataPath"].GetString()};

            metadata["images"].GetArray().Erase(imageMetadata);
            atomicallyUpdateMetadataFile(metadata);

            boost::filesystem::remove_all(imagePath);
            boost::filesystem::remove_all(metadataPath);
        }
        catch(std::exception& e) {
            auto message = boost::format("Failed to remove image %s") % imageID;
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

        // make images
        auto images = std::vector<common::SarusImage>{};

        for (const auto& imageMetadata : metadata["images"].GetArray()) {
            auto imageID = common::ImageID{
                imageMetadata["server"].GetString(),
                imageMetadata["namespace"].GetString(),
                imageMetadata["image"].GetString(),
                imageMetadata["tag"].GetString()};
            auto image = common::SarusImage{
                imageID,
                imageMetadata["digest"].GetString(),
                imageMetadata["datasize"].GetString(),
                imageMetadata["created"].GetString(),
                boost::filesystem::path{imageMetadata["imagePath"].GetString()},
                boost::filesystem::path{imageMetadata["metadataPath"].GetString()}
            };
            images.push_back(image);
        }

        printLog(boost::format("Successfully created list of images."), common::LogLevel::DEBUG);
        
        return images;
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

    rapidjson::Value ImageStore::createImageJSON(const common::SarusImage& image, rapidjson::MemoryPoolAllocator<>& allocator) const {
        try {
            auto ret = rj::Value{rj::kObjectType};
            ret.AddMember(  "uniqueKey",
                            rj::Value{image.imageID.getUniqueKey().c_str(), allocator},
                            allocator);
            ret.AddMember(  "server",
                            rj::Value{image.imageID.server.c_str(), allocator},
                            allocator);
            ret.AddMember(  "namespace",
                            rj::Value{image.imageID.repositoryNamespace.c_str(), allocator},
                            allocator);
            ret.AddMember(  "image",
                            rj::Value{image.imageID.image.c_str(), allocator},
                            allocator);
            ret.AddMember(  "tag",
                            rj::Value{image.imageID.tag.c_str(), allocator},
                            allocator);
            ret.AddMember(  "digest",
                            rj::Value{image.digest.c_str(), allocator},
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

    void ImageStore::printLog(const boost::format &message, common::LogLevel LogLevel) const {
        common::Logger::getInstance().log( message.str(), sysname, LogLevel);
    }
    
} // namespace
} // namespace
