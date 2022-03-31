/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_common_ImageMetadata_hpp
#define sarus_common_ImageMetadata_hpp

#include <string>
#include <unordered_map>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "common/CLIArguments.hpp"
#include "common/Utility.hpp"

namespace sarus {
namespace common {

class ImageMetadata {
public:
    ImageMetadata() = default;
    ImageMetadata(const boost::filesystem::path& path, const common::UserIdentity& identity);
    ImageMetadata(const rapidjson::Value& metadata);
    boost::optional<common::CLIArguments> cmd;
    boost::optional<common::CLIArguments> entry;
    boost::optional<boost::filesystem::path> workdir;
    std::unordered_map<std::string, std::string> env;
    /**
     * The "labels" term used here is in apparent contrast with the choice throughout the OCI
     * specs to name arbitrary key-value metadata for images and bundles as "annotations".
     * However, the OCI Image Specification states that the arbitrary image metadata field
     * in the image configuration JSON is named "Labels"
     * (see https://github.com/opencontainers/image-spec/blob/main/config.md?plain=1#L177);
     * this is likely a legacy from Docker image configs, which has been retained for backward
     * compatibility reasons.
     * To make the ImageMetadata class able to parse both OCI image configs and Sarus image
     * metadata in the cleanest way possible, the "Labels" term is adopted from the OCI image
     * configs into the Sarus image metadata JSON, and is also adopted as the name of the
     * class data member to avoid confusion about what the data member represents.
     */
    std::unordered_map<std::string, std::string> labels;
    void write(const boost::filesystem::path& path) const;

private:
    void parseJSON(const rapidjson::Value& json);
};

bool operator==(const ImageMetadata&, const ImageMetadata&);

}
}

#endif
