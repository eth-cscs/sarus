/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_utility_string_hpp
#define libsarus_utility_string_hpp

#include <string>
#include <tuple>
#include <unordered_map>
#include <sys/types.h>

/**
 * Utility functions for string manipulation 
 */

namespace libsarus {
namespace string {

std::string removeWhitespaces(const std::string&);
std::string replace(std::string &buf, const std::string& from, const std::string& to);
std::string eraseFirstAndLastDoubleQuote(const std::string& buf);
std::pair<std::string, std::string> parseKeyValuePair(const std::string& pairString, const char separator = '=');
std::string generateRandom(size_t size);
std::unordered_map<std::string, std::string> parseMap(const std::string& input,
                                                      const char pairSeparators = ',',
                                                      const char keyValueSeparators = '=');

}}

#endif
