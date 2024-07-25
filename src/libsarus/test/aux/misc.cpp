/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_test_aux_misc_hpp
#define libsarus_test_aux_misc_hpp

#include "misc.hpp"

#include <sstream>

#include <rapidjson/prettywriter.h>

#include "libsarus/Error.hpp"
#include "libsarus/PasswdDB.hpp"
#include "libsarus/Utility.hpp"


namespace libsarus {
namespace test {
namespace aux {
namespace misc {

std::tuple<uid_t, gid_t> getNonRootUserIds() {
    auto out = libsarus::process::executeCommand("getent passwd");
    std::stringstream ss{out};
    auto passwd = libsarus::PasswdDB{ss};

    for(const auto& entry : passwd.getEntries()) {
        if(entry.uid != 0) {
            return std::tuple<uid_t, gid_t>{entry.uid, entry.gid};
        }
    }
    
    SARUS_THROW_ERROR("Failed to find non-root user ids");
}

}}}}

#endif
