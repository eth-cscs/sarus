/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _common_utility_Environment_hpp
#define _common_utility_Environment_hpp

#include <string>
#include <tuple>
#include <unordered_map>

/**
 * Utility functions for environment variables
 */

namespace sarus {
namespace common {

std::unordered_map<std::string, std::string> parseEnvironmentVariables(char** env);
std::pair<std::string, std::string> parseEnvironmentVariable(const std::string& variable);
std::string getEnvironmentVariable(const std::string& key);
void setEnvironmentVariable(const std::string& key, const std::string& value);
void clearEnvironmentVariables();

} // namespace
} // namespace

#endif
