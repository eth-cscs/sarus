/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
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
#include "image_manager/InputImage.hpp"
#include "image_manager/ImageStore.hpp"
#include "image_manager/Puller.hpp"


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
    void processImage(const InputImage& image);
    void issueWarningIfIsCentralizedRepositoryAndIsNotRootUser() const;
    void issueErrorIfIsCentralizedRepositoryAndCentralizedRepositoryIsDisabled() const;
    void printLog(const boost::format& message, common::LogLevel LogLevel) const;

private:
    std::shared_ptr<const common::Config> config;
    Puller puller;
    ImageStore imageStore;
    const std::string sysname = "ImageManager";  // system name for logger
};

} // namespace
} // namespace

#endif
