/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <sstream>
// boost library must be included before CppUTest
#include <boost/filesystem.hpp>

#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "common/Utility.hpp"
#include "common/Lockfile.hpp"
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(CLIArgumentsTestGroup) {
};

TEST(CLIArgumentsTestGroup, serialize) {
    auto args = common::CLIArguments{"command", "arg0", "arg1"};

    std::stringstream os;
    os << args;

    CHECK_EQUAL(os.str(), std::string{"[\"command\", \"arg0\", \"arg1\"]"});
};

TEST(CLIArgumentsTestGroup, deserialize) {
    std::stringstream is("[\"command\", \"arg0\", \"arg1\"]");

    common::CLIArguments args;
    is >> args;

    auto expected = common::CLIArguments{"command", "arg0", "arg1"};
    CHECK(args == expected);
};

TEST(CLIArgumentsTestGroup, string) {
    std::stringstream is("[\"command\", \"arg0\", \"arg1\"]");

    common::CLIArguments args;
    is >> args;

    auto expected = std::string{"command arg0 arg1"};
    CHECK(args.string() == expected);
};

SARUS_UNITTEST_MAIN_FUNCTION();
