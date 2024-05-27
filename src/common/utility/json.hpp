/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _common_utility_JSON_hpp
#define _common_utility_JSON_hpp

#include <string>

#include <boost/filesystem.hpp>
#include <rapidjson/document.h>
#include <rapidjson/schema.h>

/**
 * Utility functions for JSON operations
 */

namespace sarus {
namespace common {

rapidjson::SchemaDocument readJSONSchema(const boost::filesystem::path& schemaFile);
rapidjson::Document parseJSONStream(std::istream& is);
rapidjson::Document parseJSON(const std::string& string);
rapidjson::Document readJSON(const boost::filesystem::path& filename);
rapidjson::Document readAndValidateJSON(const boost::filesystem::path& jsonFile,
                                        const boost::filesystem::path& schemaFile);
void writeJSON(const rapidjson::Value& json, const boost::filesystem::path& filename);
std::string serializeJSON(const rapidjson::Value& json);

} // namespace
} // namespace

#endif
