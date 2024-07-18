/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "libsarus/Logger.hpp"
#include "libsarus/PathRAII.hpp"
#include "libsarus/DeviceParser.hpp"
#include "DeviceParserChecker.hpp"
#include "test_utility/filesystem.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace libsarus {
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
        .expectAccess("rm");

    // wrong access flags
    DeviceParserChecker{"/dev/sarusTestDevice0:/dev/containerDevice:raw"}.expectParseError();
    DeviceParserChecker{"/dev/sarusTestDevice0:/dev/containerDevice:rww"}.expectParseError();
    DeviceParserChecker{"/dev/sarusTestDevice0:/dev/containerDevice:rwmw"}.expectParseError();

    // empty fields
    DeviceParserChecker{":/dev/sarusTestDevice0:rw"}.expectParseError();
    DeviceParserChecker{"/dev/sarusTestDevice0::rw"}.expectParseError();
    DeviceParserChecker{"/dev/sarusTestDevice0:/dev/containerDevice:"}.expectParseError();
}

#ifdef ASROOT
TEST(DeviceParserTestGroup, constructors) {
#else
IGNORE_TEST(DeviceParserTestGroup, constructors) {
#endif
    auto configRAII = test_utility::config::makeConfig();
    auto userIdentity = configRAII.config->userIdentity;
    auto rootfsDir = boost::filesystem::path{ configRAII.config->json["OCIBundleDir"].GetString() }
                     / configRAII.config->json["rootfsFolder"].GetString();

     auto requestString = std::string("/dev/sarusTestDevice0:/dev/containerDevice:r");

     auto ctor1 = DeviceParser{configRAII.config->getRootfsDirectory(), configRAII.config->userIdentity}.parseDeviceRequest(requestString);
     auto ctor2 = DeviceParser{rootfsDir, userIdentity}.parseDeviceRequest(requestString);

     CHECK(ctor1->getSource()          == ctor2->getSource());
     CHECK(ctor1->getDestination()     == ctor2->getDestination());
     CHECK(ctor1->getFlags()           == ctor2->getFlags());
     CHECK(ctor1->getAccess().string() == ctor2->getAccess().string());
}

}}

SARUS_UNITTEST_MAIN_FUNCTION();