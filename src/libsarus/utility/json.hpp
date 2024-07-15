/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_utility_json_hpp
#define libsarus_utility_json_hpp

#include <string>

#include <boost/filesystem.hpp>
#include <rapidjson/document.h>
#include <rapidjson/schema.h>

/**
 * Utility functions for JSON operations
 */

namespace libsarus {
namespace json {

rapidjson::SchemaDocument readSchema(const boost::filesystem::path& schemaFile);
rapidjson::Document parseStream(std::istream& is);
rapidjson::Document parse(const std::string& string);
rapidjson::Document read(const boost::filesystem::path& filename);
rapidjson::Document readAndValidate(const boost::filesystem::path& jsonFile,
                                        const boost::filesystem::path& schemaFile);
void write(const rapidjson::Value& json, const boost::filesystem::path& filename);
std::string serialize(const rapidjson::Value& json);

}}

#endif
