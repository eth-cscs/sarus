/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_image_manger_OCIImage_hpp
#define sarus_image_manger_OCIImage_hpp

#include <memory>
#include <boost/filesystem.hpp>

#include "common/Config.hpp"
#include "libsarus/PathRAII.hpp"
#include "common/ImageMetadata.hpp"


namespace sarus {
namespace image_manager {

class OCIImage {
public:
    OCIImage(std::shared_ptr<const common::Config> config, const boost::filesystem::path& imagePath);
    libsarus::PathRAII unpack() const;
    std::string getImageID() const {return imageID;};
    sarus::common::ImageMetadata getMetadata() const {return metadata;};
    void release();

private:
    boost::filesystem::path makeTemporaryUnpackDirectory() const;
    void log(const boost::format &message, libsarus::LogLevel,
             std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr) const;
    void log(const std::string& message, libsarus::LogLevel,
             std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr) const;

private:
    std::shared_ptr<const common::Config> config;
    libsarus::PathRAII imageDir;
    common::ImageMetadata metadata;
    std::string imageID;
};

}
}

#endif
