/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
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

#include "libsarus/PathRAII.hpp"
#include "libsarus/Error.hpp"
#include "libsarus/Logger.hpp"
#include "libsarus/Utility.hpp"
#include "libsarus/Flock.hpp"
#include "common/SarusImage.hpp"


namespace rj = rapidjson;

namespace sarus {
namespace image_manager {

    ImageStore::ImageStore(std::shared_ptr<const common::Config> config)
        : imagesDirectory{config->directories.images}
        , metadataFile{config->directories.repository / "metadata.json"}
    {
        if (!boost::filesystem::exists(metadataFile)) {
            initRepositoryMetadataFile();
        }

        auto rjPtr = rj::Pointer("/repositoryMetadataLockTimings/timeoutMs");
        if (const rj::Value* configLockTimeoutMs = rjPtr.Get(config->json)) {
            lockTimeout = milliseconds{configLockTimeoutMs->GetInt()};
        }
        else {
            lockTimeout = milliseconds{60000};
        }

        rjPtr = rj::Pointer("/repositoryMetadataLockTimings/warningMs");
        if (const rj::Value* configLockWarningMs = rjPtr.Get(config->json)) {
            lockWarning = milliseconds{configLockWarningMs->GetInt()};
        }
        else {
            lockWarning = milliseconds{10000};
        }
    }

    /**
     * Add the container image into repository (or update existing object)
     */
    void ImageStore::addImage(const common::SarusImage& image) const {
        printLog(boost::format("Adding image %s to metadata file %s") % image.reference % metadataFile,
                 libsarus::LogLevel::INFO);

        try {
            libsarus::Flock lock{metadataFile, libsarus::Flock::Type::writeLock, lockTimeout, lockWarning};
            auto metadata = libsarus::json::read(metadataFile);

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

            atomicallyUpdateRepositoryMetadataFile(metadata, &lock);
        }
        catch (const std::exception &e) {
            auto message = boost::format("Failed to add image %s to repository metadata file %s")
                                        % image.reference % metadataFile;
            SARUS_RETHROW_ERROR(e, message.str());
        }

        printLog(boost::format("Successfully added image"), libsarus::LogLevel::INFO);
    }

    /**
     * Remove container image from repository
     */
    void ImageStore::removeImage(const common::ImageReference& imageReference) const {
        printLog(boost::format("Attempting to remove image %s from local repository") % imageReference,
                 libsarus::LogLevel::INFO);

        try {
            libsarus::Flock lock{metadataFile, libsarus::Flock::Type::writeLock, lockTimeout, lockWarning};
            auto repositoryMetadata = libsarus::json::read(metadataFile);
            auto imageMetadata = findImageMetadata(imageReference, repositoryMetadata);

            if (!imageMetadata) {
                auto message = boost::format("Cannot find image '%s'") % imageReference;
                printLog(message, libsarus::LogLevel::GENERAL, std::cerr);
                SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
            }

            // Attempting to remove backing files first so that, if something goes wrong on metadata removal,
            // the orphaned metadata have more chance to be cleaned during a subsequent "sarus images" or "sarus run" command.
            // If we remove metadata first and something goes wrong on backing files removal
            // there would be no data-driven way to reach the orphaned files, which would just lie in the filesystem occupying space.
            removeImageBackingFiles(imageMetadata);
            removeRepositoryMetadataEntry(imageMetadata, repositoryMetadata, &lock);
        }
        catch(const std::exception& e) {
            auto message = boost::format("Failed to remove image %s") % imageReference;
            SARUS_RETHROW_ERROR(e, message.str());
        }

        printLog(boost::format("Successfully removed image from local repository"), libsarus::LogLevel::INFO);
    }

    /**
     * List the containers in repository
     */
    std::vector<sarus::common::SarusImage> ImageStore::listImages() const {
        auto images = std::vector<common::SarusImage>{};

        try {
            libsarus::Flock lock{metadataFile, libsarus::Flock::Type::writeLock, lockTimeout, lockWarning};
            auto repositoryMetadata = libsarus::json::read(metadataFile);
            for (const auto& imageMetadata : repositoryMetadata["images"].GetArray()) {
                // If backing files are present, all image data is available: add the image to list to be visualized.
                // Else, ensure all image data is cleaned up
                if (hasImageBackingFiles(imageMetadata)) {
                    auto image = convertImageMetadataToSarusImage(imageMetadata);
                    images.push_back(image);
                }
                else {
                    removeImageBackingFiles(&imageMetadata);
                    removeRepositoryMetadataEntry(&imageMetadata, repositoryMetadata, &lock);
                }
            }
        }
        catch(const std::exception& e) {
            auto message = boost::format("Failed to list images: %s") % e.what();
            SARUS_RETHROW_ERROR(e, message.str());
        }

        printLog(boost::format("Successfully created list of images."), libsarus::LogLevel::DEBUG);
        return images;
    }

    boost::optional<sarus::common::SarusImage> ImageStore::findImage(const common::ImageReference& reference) const {
        printLog(boost::format("Looking for reference '%s' in local repository") % reference, libsarus::LogLevel::DEBUG);
        boost::optional<common::SarusImage> image;

        try {
            libsarus::Flock lock{metadataFile, libsarus::Flock::Type::readLock, lockTimeout, lockWarning};
            auto repositoryMetadata = libsarus::json::read(metadataFile);
            auto imageMetadata = findImageMetadata(reference, repositoryMetadata);
            if (imageMetadata) {
                // If backing files are present, all image data is available: assign object to return
                // Else, ensure all image data is cleaned up
                if (hasImageBackingFiles(*imageMetadata)) {
                    image = convertImageMetadataToSarusImage(*imageMetadata);
                }
                else {
                    removeImageBackingFiles(imageMetadata);
                    // Obtain exclusive access to the file by acquiring a write lock
                    lock.convertToType(libsarus::Flock::Type::writeLock);
                    // Check if another process has updated the metadata in the meantime
                    repositoryMetadata = libsarus::json::read(metadataFile);
                    imageMetadata = findImageMetadata(reference, repositoryMetadata);
                    if (imageMetadata) {
                        removeRepositoryMetadataEntry(imageMetadata, repositoryMetadata, &lock);
                    }
                }
            }
        }
        catch(const std::exception& e) {
            auto message = boost::format("Failed to find image %s") % reference;
            SARUS_RETHROW_ERROR(e, message.str());
        }

        printLog(boost::format("Image for reference '%s' %s") % reference % (image ? "found" : "not found"),
                 libsarus::LogLevel::DEBUG);
        return image;
    }

    rapidjson::Document ImageStore::initRepositoryMetadataFile() const {
        rj::Document metadata;

        metadata = rj::Document{rj::kObjectType};
        metadata.AddMember("images", rj::kArrayType, metadata.GetAllocator());

        libsarus::Flock lock{};
        atomicallyUpdateRepositoryMetadataFile(metadata, &lock);

        return metadata;
    }

    const rapidjson::Value* ImageStore::findImageMetadata(const common::ImageReference& reference, const rapidjson::Document& metadata) const {
        printLog(boost::format("Looking for reference '%s' in repository metadata") % reference, libsarus::LogLevel::DEBUG);
        const rj::Value* imageMetadata = nullptr;
        auto uniqueKey = reference.getUniqueKey();

        for(const auto& image : metadata["images"].GetArray()) {
            if(image["uniqueKey"].GetString() == uniqueKey) {
                imageMetadata = &image;
                break;
            }
        }

        printLog(boost::format("Metadata for reference '%s' %s") % reference % (imageMetadata ? "found" : "not found"),
                 libsarus::LogLevel::DEBUG);
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
            printLog(message.str(), libsarus::LogLevel::INFO, std::cerr);
        }
        return missing.empty();
    }

    sarus::common::SarusImage ImageStore::convertImageMetadataToSarusImage(const rapidjson::Value& imageMetadata) const {
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
    void ImageStore::removeRepositoryMetadataEntry(const rapidjson::Value* imageEntry, rapidjson::Document& repositoryMetadata, libsarus::Flock* const lock) const {
        repositoryMetadata["images"].GetArray().Erase(imageEntry);
        atomicallyUpdateRepositoryMetadataFile(repositoryMetadata, lock);
        printLog("Removed image entry from repository metadata", libsarus::LogLevel::DEBUG);
    }

    /**
     * Deletes the image's individual squashfs file and metadata file
     */
    void ImageStore::removeImageBackingFiles(const rapidjson::Value* imageMetadata) const {
        auto imagePath = boost::filesystem::path{(*imageMetadata)["imagePath"].GetString()};
        auto metadataPath = boost::filesystem::path{(*imageMetadata)["metadataPath"].GetString()};
        boost::filesystem::remove_all(imagePath);
        boost::filesystem::remove_all(metadataPath);
        printLog("Removed image backing files", libsarus::LogLevel::DEBUG);
    }

    /**
     * Atomically update the repository's metadata file. Creates a temporary metadata file
     * and then atomically creates/replaces the actual metadata file by renaming the
     * temporary one.
     */
    void ImageStore::atomicallyUpdateRepositoryMetadataFile(const rapidjson::Value& metadata, libsarus::Flock* const lock) const {
        auto metadataFileTemp = libsarus::filesystem::makeUniquePathWithRandomSuffix(metadataFile);

        printLog( boost::format("Updating repository metadata file: %s") % metadataFile, libsarus::LogLevel::DEBUG);

        try {
            libsarus::json::write(metadata, metadataFileTemp);
            libsarus::Flock newLock{metadataFileTemp, libsarus::Flock::Type::writeLock, milliseconds{1000}, libsarus::Flock::noTimeout};

            // Atomically replace old metadata file.
            // After this, the process should hold locks on both file descriptors for new and old metadata files.
            boost::filesystem::rename(metadataFileTemp, metadataFile);

            // Hand over the lock for the new file to the caller function
            *lock = std::move(newLock);
        }
        catch (const std::exception &e) {
            auto message = boost::format("Failed to write metadata file %s") % metadataFile;
            SARUS_RETHROW_ERROR(e, message.str());
        }

        printLog("Successfully updated repository metadata file", libsarus::LogLevel::DEBUG);
    }

    boost::filesystem::path ImageStore::getImageSquashfsFile(const common::ImageReference& reference) const {
        auto relativePath = reference.getUniqueKey() + ".squashfs";
        return imagesDirectory / relativePath;
    }

    boost::filesystem::path ImageStore::getImageMetadataFile(const common::ImageReference& reference) const {
        auto relativePath = reference.getUniqueKey() + ".meta";
        return imagesDirectory / relativePath;
    }

    void ImageStore::printLog(const boost::format& message, libsarus::LogLevel LogLevel,
                              std::ostream& out, std::ostream& err) const {
        printLog(message.str(), LogLevel, out, err);
    }

    void ImageStore::printLog(const std::string& message, libsarus::LogLevel LogLevel,
                              std::ostream& out, std::ostream& err) const {
        libsarus::Logger::getInstance().log(message, sysname, LogLevel, out, err);
    }

} // namespace
} // namespace
