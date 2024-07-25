/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_test_aux_hook_hpp
#define libsarus_test_aux_hook_hpp

#include <tuple>
#include <sys/types.h>

#include <boost/filesystem.hpp>
#include <rapidjson/document.h>


namespace libsarus {
namespace test {
namespace aux {
namespace hook {

void writeOCIContainerStateToStdin(const boost::filesystem::path& bundleDir);
rapidjson::Document createOCIBaseConfigJSON(const boost::filesystem::path& rootfsDir, 
                                            const std::tuple<uid_t, gid_t>& idsOfUser);

}}}}

#endif
