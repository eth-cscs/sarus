/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_common_Utility_hpp
#define sarus_hooks_common_Utility_hpp

#include <tuple>
#include <string>
#include <unordered_map>
#include <sys/types.h>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

namespace sarus {
namespace hooks {
namespace common {
namespace utility {

void applyLoggingConfigIfAvailable(const rapidjson::Document&);
std::tuple<boost::filesystem::path, pid_t> parseStateOfContainerFromStdin();
std::unordered_map<std::string, std::string> parseEnvironmentVariablesFromOCIBundle(const boost::filesystem::path&);
void enterNamespacesOfProcess(pid_t);

}}}} // namespace

#endif
