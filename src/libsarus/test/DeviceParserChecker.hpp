/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_test_DeviceParserChecker_hpp
#define libsarus_test_DeviceParserChecker_hpp

#include <boost/optional.hpp>

#include "test_utility/config.hpp"
#include "libsarus/Error.hpp"
#include "libsarus/DeviceParser.hpp"

#include <CppUTest/CommandLineTestRunner.h> // boost library must be included before CppUTest

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
        auto configRAII = test_utility::config::makeConfig();
        auto userIdentity = configRAII.config->userIdentity;
        auto rootfsDir = boost::filesystem::path{ configRAII.config->json["OCIBundleDir"].GetString() }
                         / configRAII.config->json["rootfsFolder"].GetString();

        auto parser = libsarus::DeviceParser{configRAII.config->getRootfsDirectory(), configRAII.config->userIdentity};

        if(isParseErrorExpected) {
            CHECK_THROWS(libsarus::Error, parser.parseDeviceRequest(deviceRequest));
            return;
        }

        auto mountObject = parser.parseDeviceRequest(deviceRequest);

        if(expectedSource) {
            CHECK(mountObject->source == *expectedSource);
        }

        if(expectedDestination) {
            CHECK(mountObject->destination == *expectedDestination);
        }

        if(expectedAccess) {
            CHECK(mountObject->getAccess().string() == *expectedAccess);
        }

        CHECK_EQUAL(mountObject->mountFlags, *expectedFlags);
    }

private:
    std::string deviceRequest;
    boost::optional<std::string> expectedSource{};
    boost::optional<std::string> expectedDestination{};
    boost::optional<std::string> expectedAccess{"rwm"};
    boost::optional<size_t> expectedFlags{MS_REC | MS_PRIVATE};

    bool isParseErrorExpected = false;
};

}}

#endif
