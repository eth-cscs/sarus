/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_image_manager_ImageStore_hpp
#define sarus_image_manager_ImageStore_hpp

#include <time.h>
#include <vector>
#include <string>
#include <memory>

#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "common/Config.hpp"
#include "common/SarusImage.hpp"


namespace sarus {
namespace image_manager {

class ImageStore {
public:
    ImageStore(std::shared_ptr<const common::Config>);

    void addImage(const common::SarusImage&);
    void removeImage(const common::ImageID&);
    std::vector<common::SarusImage> listImages() const;
    const boost::filesystem::path& getMetadataFile() const { return metadataFile; }

private:
    rapidjson::Document readRepositoryMetadata() const;
    void atomicallyUpdateMetadataFile(const rapidjson::Value& metadata) const;
    rapidjson::Value createImageJSON(const common::SarusImage&, rapidjson::MemoryPoolAllocator<>& allocator) const;
    void printLog(  const boost::format &message, common::LogLevel LogLevel,
                    std::ostream& out = std::cout, std::ostream& err = std::cerr) const;

private:
    const std::string sysname = "ImageStore"; // system name for logger
    std::shared_ptr<const common::Config> config;
    boost::filesystem::path metadataFile;
};

} // namespace
} // namespace

#endif
