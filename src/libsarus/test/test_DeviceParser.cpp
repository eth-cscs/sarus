/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "aux/filesystem.hpp"
#include "aux/unitTestMain.hpp"
#include "libsarus/DeviceParser.hpp"
#include "libsarus/Logger.hpp"
#include "libsarus/PathRAII.hpp"
#include "libsarus/Utility.hpp"


namespace libsarus {
namespace test {

class DeviceParserChecker {
public:
    DeviceParserChecker(const std::string& deviceRequest)
        : deviceRequest(deviceRequest)
    {}

    DeviceParserChecker& expectSource(const std::string& expectedSource) {
        this->expectedSource = expectedSource;
        return *this;
    }

    DeviceParserChecker& expectDestination(const std::string& expectedDestination) {
        this->expectedDestination = expectedDestination;
        return *this;
    }

    DeviceParserChecker& expectAccess(const std::string& expectedAccess) {
        this->expectedAccess = expectedAccess;
        return *this;
    }

    DeviceParserChecker& expectParseError() {
        isParseErrorExpected = true;
        return *this;
    }

    ~DeviceParserChecker() {
        libsarus::UserIdentity userIdentity;
        auto bundleDirRAII = libsarus::PathRAII{libsarus::filesystem::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("test-bundle-dir"))};
        auto rootfsDir = bundleDirRAII.getPath() / "rootfs";

        auto parser = libsarus::DeviceParser{rootfsDir, userIdentity};

        if(isParseErrorExpected) {
            CHECK_THROWS(libsarus::Error, parser.parseDeviceRequest(deviceRequest));
            return;
        }

        auto mountObject = parser.parseDeviceRequest(deviceRequest);

        if(expectedSource) {
            CHECK(mountObject->getSource() == *expectedSource);
        }

        if(expectedDestination) {
            CHECK(mountObject->getDestination() == *expectedDestination);
        }

        if(expectedAccess) {
            CHECK(mountObject->getAccess().string() == *expectedAccess);
        }

        CHECK_EQUAL(mountObject->getFlags(), *expectedFlags);
    }

private:
    std::string deviceRequest;
    boost::optional<std::string> expectedSource{};
    boost::optional<std::string> expectedDestination{};
    boost::optional<std::string> expectedAccess{"rwm"};
    boost::optional<size_t> expectedFlags{MS_REC | MS_PRIVATE};

    bool isParseErrorExpected = false;
};

TEST_GROUP(DeviceParserTestGroup) {
    void setup() {
        auto testDevice = boost::filesystem::path("/dev/sarusTestDevice0");
        auto testDeviceMajorID = 511u;
        auto testDeviceMinorID = 511u;
        if (boost::filesystem::exists(testDevice)) {
            boost::filesystem::remove(testDevice);
        }
        aux::filesystem::createCharacterDeviceFile(testDevice, testDeviceMajorID, testDeviceMinorID);
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

}}

SARUS_UNITTEST_MAIN_FUNCTION();
