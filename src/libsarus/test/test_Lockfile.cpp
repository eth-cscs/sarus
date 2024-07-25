/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <type_traits>

#include <boost/filesystem.hpp>

#include "aux/unitTestMain.hpp"
#include "libsarus/Error.hpp"
#include "libsarus/Lockfile.hpp"
#include "libsarus/Utility.hpp"


namespace libsarus {
namespace test {

TEST_GROUP(LockfileTestGroup) {
    boost::filesystem::path fileToLock = libsarus::filesystem::makeUniquePathWithRandomSuffix("/tmp/file-to-lock");
    boost::filesystem::path lockfile = fileToLock.string() + ".lock";
};

TEST(LockfileTestGroup, creation_of_physical_lockfile) {
    CHECK(!boost::filesystem::exists(lockfile));
    libsarus::Lockfile lock{fileToLock};
    CHECK(boost::filesystem::exists(lockfile));
}

TEST(LockfileTestGroup, lock_acquisition) {
    {
        libsarus::Lockfile lock{fileToLock};
    }
    {
        // check that we can reacquire the lock
        // (previous lock was released when went out of scope)
        libsarus::Lockfile lock{fileToLock};
    }
    {
        libsarus::Lockfile lock{fileToLock};
        // check that lock cannot be acquired more than once
        CHECK_THROWS(libsarus::Error, libsarus::Lockfile(fileToLock, 0));
        // even if we try again...
        CHECK_THROWS(libsarus::Error, libsarus::Lockfile(fileToLock, 0));
    }
}

TEST(LockfileTestGroup, move_constructor) {
    libsarus::Lockfile original{fileToLock};
    {
        libsarus::Lockfile moveConstructed{std::move(original)};
        // check that lock cannot be acquired more than once (move constructed lock is still active)
        CHECK_THROWS(libsarus::Error, libsarus::Lockfile(fileToLock, 0));
    }
    // check that lock can be acquired (move-constructed lock went out of scope)
    libsarus::Lockfile newlock{fileToLock};
}

TEST(LockfileTestGroup, move_assignment) {
    libsarus::Lockfile original{fileToLock};
    {
        libsarus::Lockfile moveAssigned;
        moveAssigned = std::move(original);
        // check that lock cannot be acquired more than once (move assigned lock is still active)
        CHECK_THROWS(libsarus::Error, libsarus::Lockfile(fileToLock, 0));
    }
    // check that lock can be acquired (move-assigned lock went out of scope)
    libsarus::Lockfile newlock{fileToLock};
}

static_assert(!std::is_copy_constructible<libsarus::Lockfile>::value, "");
static_assert(!std::is_copy_assignable<libsarus::Lockfile>::value, "");
static_assert(std::is_move_constructible<libsarus::Lockfile>::value, "");
static_assert(std::is_move_assignable<libsarus::Lockfile>::value, "");

}}

SARUS_UNITTEST_MAIN_FUNCTION();
