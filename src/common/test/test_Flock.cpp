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

#include "common/Error.hpp"
#include "common/Flock.hpp"
#include "common/Utility.hpp"

#include "test_utility/unittest_main_function.hpp"
#include <boost/filesystem.hpp>

using namespace sarus;

constexpr std::chrono::milliseconds operator""_ms(unsigned long long ms) {
    return std::chrono::milliseconds(ms);
}

static bool lockAcquisitionDoesntThrow(const boost::filesystem::path &fileToLock, const common::Flock::Type &lockType) {
    try {
        common::Flock{fileToLock, lockType, 10_ms};
    } catch (const common::Error &) {
        return false;
    }
    return true;
}

TEST_GROUP(FlockTestGroup) {
    boost::filesystem::path fileToLock = common::makeUniquePathWithRandomSuffix("/tmp/file-to-lock");
    boost::filesystem::path lockfile = fileToLock.string() + ".lock";

    void setup() {
        common::createFileIfNecessary(fileToLock);
    }

    void teardown() {
        boost::filesystem::remove(fileToLock);
    }
};

TEST(FlockTestGroup, lock_is_released_when_the_object_is_destroyed) {
    {
        common::Logger::getInstance().setLevel(common::LogLevel::DEBUG);
        common::Flock lock{fileToLock, common::Flock::Type::writeLock};
    }
    {
        CHECK(lockAcquisitionDoesntThrow(fileToLock, common::Flock::Type::writeLock));
    }
    {
        CHECK(lockAcquisitionDoesntThrow(fileToLock, common::Flock::Type::writeLock));
    }
}

TEST(FlockTestGroup, move_constructor_moves_resources) {
    common::Flock original{fileToLock, common::Flock::Type::writeLock};
    {
        common::Flock moveConstructed{std::move(original)};
        CHECK_THROWS(common::Error, common::Flock(fileToLock, common::Flock::Type::writeLock, 10_ms));
    }
    // check that lock can be acquired (move-constructed lock went out of scope)
    CHECK(lockAcquisitionDoesntThrow(fileToLock, common::Flock::Type::writeLock));
}

TEST(FlockTestGroup, move_assignment_moves_resources) {
    common::Flock original{fileToLock, common::Flock::Type::writeLock};
    {
        common::Flock moveAssigned;
        moveAssigned = std::move(original);
        CHECK_THROWS(common::Error, common::Flock(fileToLock, common::Flock::Type::writeLock, 10_ms));
    }
    // check that lock can be acquired (move-assigned lock went out of scope)
    CHECK(lockAcquisitionDoesntThrow(fileToLock, common::Flock::Type::writeLock));
}

TEST(FlockTestGroup, write_fails_if_resource_is_in_use) {
    {
        common::Flock lock{fileToLock, common::Flock::Type::writeLock};
        CHECK_THROWS(common::Error, common::Flock(fileToLock, common::Flock::Type::writeLock, 10_ms));
    }
    {
        common::Flock lock{fileToLock, common::Flock::Type::readLock};
        CHECK_THROWS(common::Error, common::Flock(fileToLock, common::Flock::Type::writeLock, 10_ms));
    }
}

TEST(FlockTestGroup, concurrent_read_are_allowed) {
    common::Flock lock{fileToLock, common::Flock::Type::readLock};
    CHECK(lockAcquisitionDoesntThrow(fileToLock, common::Flock::Type::readLock));
}

TEST(FlockTestGroup, read_fails_if_resource_is_being_written) {
    common::Flock lock{fileToLock, common::Flock::Type::writeLock};
    CHECK_THROWS(common::Error, common::Flock(fileToLock, common::Flock::Type::readLock, 10_ms));
}

TEST(FlockTestGroup, convert_read_to_write) {
    common::Flock lock{fileToLock, common::Flock::Type::readLock};
    lock.convertToType(common::Flock::Type::writeLock);
    CHECK_THROWS(common::Error, common::Flock(fileToLock, common::Flock::Type::readLock, 10_ms));
}

TEST(FlockTestGroup, convert_write_to_read) {
    common::Flock lock{fileToLock, common::Flock::Type::writeLock};
    lock.convertToType(common::Flock::Type::readLock);
    CHECK(lockAcquisitionDoesntThrow(fileToLock, common::Flock::Type::readLock));
}

TEST(FlockTestGroup, timeout_time_is_respected) {
    common::Flock lock{fileToLock, common::Flock::Type::writeLock};

    for (auto &timeout : {10, 100, 500, 1000, 2000}) {
        auto start = std::chrono::system_clock::now();
        try {
            common::Flock{fileToLock, common::Flock::Type::writeLock, std::chrono::milliseconds{timeout}};
        } catch (const common::Error &) {
        }
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        CHECK(timeout <= elapsed.count() <= 2 * timeout);
    }
}

static_assert(!std::is_copy_constructible<common::Flock>::value, "");
static_assert(!std::is_copy_assignable<common::Flock>::value, "");
static_assert(std::is_move_constructible<common::Flock>::value, "");
static_assert(std::is_move_assignable<common::Flock>::value, "");

SARUS_UNITTEST_MAIN_FUNCTION();
