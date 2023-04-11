/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/DeviceAccess.hpp"
#include "test_utility/unittest_main_function.hpp"


namespace sarus {
namespace common {
namespace test {

TEST_GROUP(DeviceAccessTestGroup) {
};

TEST(DeviceAccessTestGroup, valid_inputs) {
    auto access = DeviceAccess("rwm");
    CHECK(access.string() == "rwm");

    access = DeviceAccess("wmr");
    CHECK(access.string() == "rwm");

    access = DeviceAccess("r");
    CHECK(access.string() == "r");

    access = DeviceAccess("w");
    CHECK(access.string() == "w");

    access = DeviceAccess("m");
    CHECK(access.string() == "m");

    access = DeviceAccess("rw");
    CHECK(access.string() == "rw");

    access = DeviceAccess("wr");
    CHECK(access.string() == "rw");

    access = DeviceAccess("mr");
    CHECK(access.string() == "rw");

    access = DeviceAccess("wm");
    CHECK(access.string() == "wm");

    access = DeviceAccess("mw");
    CHECK(access.string() == "wm");
}

TEST(DeviceAccessTestGroup, invalid_inputs) {
    // empty string
    CHECK_THROWS(common::Error, DeviceAccess(""));

    //string longer than 3 characters
    CHECK_THROWS(common::Error, DeviceAccess("rwma"));

    // characters outside 'rwm'
    CHECK_THROWS(common::Error, DeviceAccess("rwa"));
    CHECK_THROWS(common::Error, DeviceAccess("zw"));
    CHECK_THROWS(common::Error, DeviceAccess("rpm"));
    CHECK_THROWS(common::Error, DeviceAccess("r&m"));
    CHECK_THROWS(common::Error, DeviceAccess("2w"));

    // repeated characters
    CHECK_THROWS(common::Error, DeviceAccess("rr"));
    CHECK_THROWS(common::Error, DeviceAccess("rrr"));
    CHECK_THROWS(common::Error, DeviceAccess("rww"));
    CHECK_THROWS(common::Error, DeviceAccess("rwr"));
    CHECK_THROWS(common::Error, DeviceAccess("wmm"));

    // capitals of valid characters
    CHECK_THROWS(common::Error, DeviceAccess("R"));
    CHECK_THROWS(common::Error, DeviceAccess("W"));
    CHECK_THROWS(common::Error, DeviceAccess("M"));
    CHECK_THROWS(common::Error, DeviceAccess("RW"));
    CHECK_THROWS(common::Error, DeviceAccess("RWM"));
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
