/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

// NOTE: Boost library must be included before CppUTest

#include <sstream>

#include <boost/filesystem.hpp>

#include "aux/unitTestMain.hpp"
#include "libsarus/Error.hpp"
#include "libsarus/Lockfile.hpp"
#include "libsarus/Logger.hpp"
#include "libsarus/Utility.hpp"


namespace libsarus {
namespace test {

TEST_GROUP(CLIArgumentsTestGroup) {
};

TEST(CLIArgumentsTestGroup, serialize) {
    auto args = libsarus::CLIArguments{"command", "arg0", "arg1"};

    std::stringstream os;
    os << args;

    CHECK_EQUAL(os.str(), std::string{"[\"command\", \"arg0\", \"arg1\"]"});
};

TEST(CLIArgumentsTestGroup, deserialize) {
    std::stringstream is("[\"command\", \"arg0\", \"arg1\"]");

    libsarus::CLIArguments args;
    is >> args;

    auto expected = libsarus::CLIArguments{"command", "arg0", "arg1"};
    CHECK(args == expected);
};

TEST(CLIArgumentsTestGroup, string) {
    std::stringstream is("[\"command\", \"arg0\", \"arg1\"]");

    libsarus::CLIArguments args;
    is >> args;

    auto expected = std::string{"command arg0 arg1"};
    CHECK(args.string() == expected);
};

}}

SARUS_UNITTEST_MAIN_FUNCTION();
