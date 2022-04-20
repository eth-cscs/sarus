/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _ImageManager_hpp
#define _ImageManager_hpp

#include <vector>
#include <string>

#include "common/Config.hpp"
#include "common/Logger.hpp"
#include "common/SarusImage.hpp"
#include "image_manager/OCIImage.hpp"
#include "image_manager/ImageStore.hpp"
#include "image_manager/SkopeoDriver.hpp"


namespace sarus {
namespace image_manager {

class ImageManager {    
public:        
    ImageManager(std::shared_ptr<const common::Config> config);
    void pullImage();
    void loadImage(const boost::filesystem::path& archive);
    void removeImage();
    std::vector<common::SarusImage> listImages() const;

private:
    void processImage(const OCIImage& image, const std::string& digest);
    std::string retrieveRegistryDigest() const;
    void issueWarningIfIsCentralizedRepositoryAndIsNotRootUser() const;
    void issueErrorIfIsCentralizedRepositoryAndCentralizedRepositoryIsDisabled() const;
    void issueErrorIfPullingByDigest() const;
    void printLog(const boost::format& message, common::LogLevel LogLevel,
                  std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr) const;
    void printLog(const std::string& message, common::LogLevel LogLevel,
            std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr) const;

private:
    std::shared_ptr<const common::Config> config;
    SkopeoDriver skopeoDriver;
    ImageStore imageStore;
    const std::string sysname = "ImageManager";  // system name for logger
};

} // namespace
} // namespace

#endif
