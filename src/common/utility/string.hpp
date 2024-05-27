/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _common_utility_String_hpp
#define _common_utility_String_hpp

#include <string>
#include <tuple>
#include <unordered_map>
#include <sys/types.h>

/**
 * Utility functions for string manipulation 
 */

namespace sarus {
namespace common {

std::string removeWhitespaces(const std::string&);
std::string replaceString(std::string &buf, const std::string& from, const std::string& to);
std::string eraseFirstAndLastDoubleQuote(const std::string& buf);
std::pair<std::string, std::string> parseKeyValuePair(const std::string& pairString, const char separator = '=');
std::string generateRandomString(size_t size);
std::unordered_map<std::string, std::string> parseMap(const std::string& input,
                                                      const char pairSeparators = ',',
                                                      const char keyValueSeparators = '=');

} // namespace
} // namespace

#endif
