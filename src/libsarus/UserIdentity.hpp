/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_UserIdentity_hpp
#define libsarus_UserIdentity_hpp

#include <vector>
#include <sys/types.h>

namespace libsarus {

struct UserIdentity {
    UserIdentity();
    UserIdentity(uid_t, gid_t, const std::vector<gid_t>&);
    uid_t uid;
    gid_t gid;
    std::vector<gid_t> supplementaryGids;
};

}

#endif
