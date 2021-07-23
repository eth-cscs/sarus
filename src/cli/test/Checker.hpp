/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_cli_test_Checker_hpp
#define sarus_cli_test_Checker_hpp

#include <memory>
#include <algorithm>
#include <string>
#include <vector>

#include <boost/optional.hpp>

#include "test_utility/config.hpp"
#include "common/Utility.hpp"
#include "common/Config.hpp"
#include "common/Error.hpp"
#include "cli/MountParser.hpp"
#include "runtime/Mount.hpp"

#include <CppUTest/CommandLineTestRunner.h> // boost library must be included before CppUTest


namespace sarus {
namespace cli {
namespace custom_mounts {
namespace test {

class Checker {
public:
    Checker(const std::string& mountRequest)
        : mountRequest(mountRequest)
    {}

    Checker& parseAsSiteMount() {
        this->isSiteMount = true;
        return *this;
    }

    Checker& expectSource(const std::string& expectedSource) {
        this->expectedSource = expectedSource;
        return *this;
    }

    Checker& expectDestination(const std::string& expectedDestination) {
        this->expectedDestination = expectedDestination;
        return *this;
    }

    Checker& expectFlags(const size_t flags) {
        this->expectedFlags = flags;
        return *this;
    }

    Checker& expectParseError() {
        isParseErrorExpected = true;
        return *this;
    }

    ~Checker() {
        auto configRAII = test_utility::config::makeConfig();
        auto mountObject = std::unique_ptr<runtime::Mount>{};

        auto parser = cli::MountParser{!isSiteMount, configRAII.config};

        auto map = common::parseMap(mountRequest);

        if(isParseErrorExpected) {
            CHECK_THROWS(sarus::common::Error, parser.parseMountRequest(map));
            return;
        }

        mountObject = parser.parseMountRequest(map);

        if(expectedSource) {
            CHECK(mountObject->source == *expectedSource);
        }

        if(expectedDestination) {
            CHECK(mountObject->destination == *expectedDestination);
        }

        if(expectedFlags) {
            CHECK_EQUAL(mountObject->mountFlags, *expectedFlags);
        }
    }

private:
    std::string mountRequest;
    bool isSiteMount = false;
    boost::optional<std::string> expectedSource = {};
    boost::optional<std::string> expectedDestination = {};
    boost::optional<size_t> expectedFlags = {};

    bool isParseErrorExpected = false;
};

} // namespace
} // namespace
} // namespace
} // namespace

#endif
