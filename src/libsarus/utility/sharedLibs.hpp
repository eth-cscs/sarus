/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_utility_sharedLibs_hpp
#define libsarus_utility_sharedLibs_hpp

#include <vector>
#include <string>

#include <boost/filesystem.hpp>

/**
 * Utility functions for shared libraries
 */

namespace libsarus {
namespace sharedlibs {

boost::filesystem::path getLinkerName(const boost::filesystem::path& path);
std::vector<boost::filesystem::path> getListFromDynamicLinker(
    const boost::filesystem::path& ldconfigPath,
    const boost::filesystem::path& rootDir);
std::vector<std::string> parseAbi(const boost::filesystem::path& lib);
std::vector<std::string> resolveAbi(const boost::filesystem::path& lib,
                                    const boost::filesystem::path& rootDir = "/");
std::string getSoname(const boost::filesystem::path& path, const boost::filesystem::path& readelfPath);
bool is64bitSharedLib(const boost::filesystem::path& path, const boost::filesystem::path& readelfPath);

}}

#endif
