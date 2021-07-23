/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_image_manger_PulledImage_hpp
#define sarus_image_manger_PulledImage_hpp

#include <vector>
#include <memory>
#include <boost/filesystem.hpp>
#include <cpprest/json.h>

#include "InputImage.hpp"


namespace sarus {
namespace image_manager {

/**
 * This class represents a pulled image that has not been expanded yet.
 */
class PulledImage : public InputImage {
public:
    PulledImage(std::shared_ptr<const common::Config> config, web::json::value& manifest);
    std::tuple<common::PathRAII, common::ImageMetadata, std::string> expand() const override;

private:
    void initializeListOfLayersAndMetadata(web::json::value &manifest);

private:
    std::vector<boost::filesystem::path> layers;
    common::ImageMetadata metadata;
    std::string digest;
};

}
}

#endif
