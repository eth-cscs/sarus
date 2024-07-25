/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "aux/unitTestMain.hpp"
#include "libsarus/DeviceAccess.hpp"


namespace libsarus {
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
    CHECK_THROWS(libsarus::Error, DeviceAccess(""));

    //string longer than 3 characters
    CHECK_THROWS(libsarus::Error, DeviceAccess("rwma"));

    // characters outside 'rwm'
    CHECK_THROWS(libsarus::Error, DeviceAccess("rwa"));
    CHECK_THROWS(libsarus::Error, DeviceAccess("zw"));
    CHECK_THROWS(libsarus::Error, DeviceAccess("rpm"));
    CHECK_THROWS(libsarus::Error, DeviceAccess("r&m"));
    CHECK_THROWS(libsarus::Error, DeviceAccess("2w"));

    // repeated characters
    CHECK_THROWS(libsarus::Error, DeviceAccess("rr"));
    CHECK_THROWS(libsarus::Error, DeviceAccess("rrr"));
    CHECK_THROWS(libsarus::Error, DeviceAccess("rww"));
    CHECK_THROWS(libsarus::Error, DeviceAccess("rwr"));
    CHECK_THROWS(libsarus::Error, DeviceAccess("wmm"));

    // capitals of valid characters
    CHECK_THROWS(libsarus::Error, DeviceAccess("R"));
    CHECK_THROWS(libsarus::Error, DeviceAccess("W"));
    CHECK_THROWS(libsarus::Error, DeviceAccess("M"));
    CHECK_THROWS(libsarus::Error, DeviceAccess("RW"));
    CHECK_THROWS(libsarus::Error, DeviceAccess("RWM"));
}

}}

SARUS_UNITTEST_MAIN_FUNCTION();
