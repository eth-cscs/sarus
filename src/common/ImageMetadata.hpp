/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
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
    void write(const boost::filesystem::path& path) const;

private:
    void parseJSON(const rapidjson::Value& json);
};

bool operator==(const ImageMetadata&, const ImageMetadata&);

}
}

#endif
