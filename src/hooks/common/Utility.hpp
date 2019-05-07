/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_common_Utility_hpp
#define sarus_hooks_common_Utility_hpp

#include <tuple>
#include <sys/types.h>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

namespace sarus {
namespace hooks {
namespace common {
namespace utility {

std::tuple<boost::filesystem::path, pid_t> parseStateOfContainerFromStdin();
void enterNamespacesOfProcess(pid_t);

} // namespace utility

namespace test {

rapidjson::Document createBaseConfigJSON(const boost::filesystem::path& rootfsDir, const std::tuple<uid_t, gid_t>& idsOfUser);

}}}} // namespace

#endif
