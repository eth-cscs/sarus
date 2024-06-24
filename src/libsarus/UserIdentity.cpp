/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "UserIdentity.hpp"

#include <cstring>
#include <unistd.h>
#include <boost/format.hpp>

#include "libsarus/Error.hpp"

namespace libsarus {

UserIdentity::UserIdentity() {
    // store current uid + gid
    uid = getuid();
    gid = getgid();

    // store current supplementary gids (if any)
    auto numOfSupplementaryGids = getgroups(0, NULL);
    if (numOfSupplementaryGids == -1) {
        auto message = boost::format("Failed to getgroups: %s") % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    if(numOfSupplementaryGids > 0) {
        supplementaryGids = std::vector<gid_t>(numOfSupplementaryGids);
        if (getgroups(supplementaryGids.size(), supplementaryGids.data()) == -1) {
            auto message = boost::format("Failed to getgroups: %s") % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }
    }
}

UserIdentity::UserIdentity(uid_t uid, gid_t gid, const std::vector<gid_t>& supplementaryGids)
    : uid{uid}
    , gid{gid}
    , supplementaryGids(supplementaryGids)
{}

}
