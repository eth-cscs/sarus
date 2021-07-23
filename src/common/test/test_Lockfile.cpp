/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <type_traits>

#include "common/Error.hpp"
#include "common/Utility.hpp"
#include "common/Lockfile.hpp"

#include <boost/filesystem.hpp>
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(LockfileTestGroup) {
    boost::filesystem::path fileToLock = common::makeUniquePathWithRandomSuffix("/tmp/file-to-lock");
    boost::filesystem::path lockfile = fileToLock.string() + ".lock";
};

TEST(LockfileTestGroup, creation_of_physical_lockfile) {
    CHECK(!boost::filesystem::exists(lockfile));
    common::Lockfile lock{fileToLock};
    CHECK(boost::filesystem::exists(lockfile));
}

TEST(LockfileTestGroup, lock_acquisition) {
    {
        common::Lockfile lock{fileToLock};
    }
    {
        // check that we can reacquire the lock
        // (previous lock was released when went out of scope)
        common::Lockfile lock{fileToLock};
    }
    {
        common::Lockfile lock{fileToLock};
        // check that lock cannot be acquired more than once
        CHECK_THROWS(common::Error, common::Lockfile(fileToLock, 0));
        // even if we try again...
        CHECK_THROWS(common::Error, common::Lockfile(fileToLock, 0));
    }
}

TEST(LockfileTestGroup, move_constructor) {
    common::Lockfile original{fileToLock};
    {
        common::Lockfile moveConstructed{std::move(original)};
        // check that lock cannot be acquired more than once (move constructed lock is still active)
        CHECK_THROWS(common::Error, common::Lockfile(fileToLock, 0));
    }
    // check that lock can be acquired (move-constructed lock went out of scope)
    common::Lockfile newlock{fileToLock};
}

TEST(LockfileTestGroup, move_assignment) {
    common::Lockfile original{fileToLock};
    {
        common::Lockfile moveAssigned;
        moveAssigned = std::move(original);
        // check that lock cannot be acquired more than once (move assigned lock is still active)
        CHECK_THROWS(common::Error, common::Lockfile(fileToLock, 0));
    }
    // check that lock can be acquired (move-assigned lock went out of scope)
    common::Lockfile newlock{fileToLock};
}

static_assert(!std::is_copy_constructible<common::Lockfile>::value, "");
static_assert(!std::is_copy_assignable<common::Lockfile>::value, "");
static_assert(std::is_move_constructible<common::Lockfile>::value, "");
static_assert(std::is_move_assignable<common::Lockfile>::value, "");

SARUS_UNITTEST_MAIN_FUNCTION();
