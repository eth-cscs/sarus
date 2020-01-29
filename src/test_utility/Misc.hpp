/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_test_utility_Misc_hpp
#define sarus_test_utility_Misc_hpp

#include "common/Error.hpp"
#include "common/Utility.hpp"
#include "common/PasswdDB.hpp"


namespace test_utility {
namespace misc {

std::tuple<uid_t, gid_t> getNonRootUserIds() {
    auto out = sarus::common::executeCommand("getent passwd");
    std::stringstream ss{out};

    auto passwd = sarus::common::PasswdDB{};
    passwd.read(ss);

    for(const auto& entry : passwd.getEntries()) {
        if(entry.uid != 0) {
            return std::tuple<uid_t, gid_t>{entry.uid, entry.gid};
        }
    }
    
    SARUS_THROW_ERROR("Failed to find non-root user ids");
}

}
}

#endif
