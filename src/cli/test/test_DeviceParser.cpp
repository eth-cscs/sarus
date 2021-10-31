/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/Logger.hpp"
#include "common/PathRAII.hpp"
#include "cli/DeviceParser.hpp"
#include "DeviceParserChecker.hpp"
#include "test_utility/filesystem.hpp"
#include "test_utility/unittest_main_function.hpp"


namespace sarus {
namespace cli {
namespace test {

TEST_GROUP(DeviceParserTestGroup) {
    void setup() {
        auto testDevice = boost::filesystem::path("/dev/sarusTestDevice0");
        auto testDeviceMajorID = 511u;
        auto testDeviceMinorID = 511u;
        if (boost::filesystem::exists(testDevice)) {
            boost::filesystem::remove(testDevice);
        }
        test_utility::filesystem::createCharacterDeviceFile(testDevice, testDeviceMajorID, testDeviceMinorID);
    }

    void teardown() {
        auto testDevice = boost::filesystem::path("/dev/sarusTestDevice0");
        boost::filesystem::remove(testDevice);
    }

};

#ifdef ASROOT
TEST(DeviceParserTestGroup, basic_checks) {
#else
IGNORE_TEST(DeviceParserTestGroup, basic_checks) {
#endif
    // empty request
    DeviceParserChecker{""}.expectParseError();

    // too many tokens
    DeviceParserChecker{"/dev/sarusTestDevice0:/dev/device1:/dev/device2:rw"}.expectParseError();
    DeviceParserChecker{"/dev/sarusTestDevice0:/dev/device1:/dev/device2:/dev/device3:rw"}.expectParseError();
}

#ifdef ASROOT
TEST(DeviceParserTestGroup, source_and_destination) {
#else
IGNORE_TEST(DeviceParserTestGroup, source_and_destination) {
#endif
    // only source path provided
    DeviceParserChecker{"/dev/sarusTestDevice0"}
        .expectSource("/dev/sarusTestDevice0")
        .expectDestination("/dev/sarusTestDevice0");

    // source and destination provided
    DeviceParserChecker{"/dev/sarusTestDevice0:/dev/container-Device"}
        .expectSource("/dev/sarusTestDevice0")
        .expectDestination("/dev/container-Device");

    // only absolute paths allowed
    DeviceParserChecker{"dev/sarusTestDevice0:/dev/containerDevice"}.expectParseError();
    DeviceParserChecker{"/dev/sarusTestDevice0:dev/containerDevice"}.expectParseError();

    // empty source or destination
    DeviceParserChecker{"/dev/sarusTestDevice0:"}.expectParseError();
    DeviceParserChecker{":/dev/containerDevice"}.expectParseError();
    DeviceParserChecker{":"}.expectParseError();
}

#ifdef ASROOT
TEST(DeviceParserTestGroup, access) {
#else
IGNORE_TEST(DeviceParserTestGroup, access) {
#endif
    // only source path provided
    DeviceParserChecker{"/dev/sarusTestDevice0:rw"}
        .expectSource("/dev/sarusTestDevice0")
        .expectDestination("/dev/sarusTestDevice0")
        .expectAccess("rw");

    // source and destination provided
    DeviceParserChecker{"/dev/sarusTestDevice0:/dev/containerDevice:r"}
        .expectSource("/dev/sarusTestDevice0")
        .expectDestination("/dev/containerDevice")
        .expectAccess("r");
    DeviceParserChecker{"/dev/sarusTestDevice0:/dev/containerDevice:mr"}
        .expectSource("/dev/sarusTestDevice0")
        .expectDestination("/dev/containerDevice")
        .expectAccess("mr");

    // wrong access flags
    DeviceParserChecker{"/dev/sarusTestDevice0:/dev/containerDevice:raw"}.expectParseError();
    DeviceParserChecker{"/dev/sarusTestDevice0:/dev/containerDevice:rww"}.expectParseError();
    DeviceParserChecker{"/dev/sarusTestDevice0:/dev/containerDevice:rwmw"}.expectParseError();

    // empty fields
    DeviceParserChecker{":/dev/sarusTestDevice0:rw"}.expectParseError();
    DeviceParserChecker{"/dev/sarusTestDevice0::rw"}.expectParseError();
    DeviceParserChecker{"/dev/sarusTestDevice0:/dev/containerDevice:"}.expectParseError();
}

} // namespace
} // namespace
} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
