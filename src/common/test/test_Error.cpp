/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <exception>

#include "common/Error.hpp"
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(ErrorTestGroup) {
};

void functionThatThrows() {
    SARUS_THROW_ERROR("first error message");
}

void functionThatRethrows() {
    try {
        functionThatThrows();
    }
    catch(common::Error& error) {
        SARUS_RETHROW_ERROR(error, "second error message");
    }
}

void functionThatThrowsFromStdException() {
    auto stdException = std::runtime_error("first error message");
    const auto& ref = stdException;
    SARUS_RETHROW_ERROR(ref, "second error message");
}

TEST(ErrorTestGroup, oneStackTraceEntry) {
    try {
        functionThatThrows();
    }
    catch(const common::Error& error) {
        auto expectedFirstEntry = common::Error::ErrorTraceEntry{"first error message", "test_Error.cpp", 12, "functionThatThrows"};

        CHECK_EQUAL(error.getErrorTrace().size(), 1);
        CHECK(error.getErrorTrace()[0] == expectedFirstEntry);
    }
}

TEST(ErrorTestGroup, twoStackTraceEntries) {
    try {
        functionThatRethrows();
    }
    catch (const common::Error& error) {
        auto expectedFirstEntry = common::Error::ErrorTraceEntry{"first error message", "test_Error.cpp", 12, "functionThatThrows"};
        auto expectedSecondEntry = common::Error::ErrorTraceEntry{"second error message", "test_Error.cpp", 20, "functionThatRethrows"};

        CHECK_EQUAL(error.getErrorTrace().size(), 2);
        CHECK(error.getErrorTrace()[0] == expectedFirstEntry);
        CHECK(error.getErrorTrace()[1] == expectedSecondEntry);        
    }
}

TEST(ErrorTestGroup, fromStdException) {
    try {
        functionThatThrowsFromStdException();
    }
    catch(const common::Error& error) {
        auto expectedFirstEntry = common::Error::ErrorTraceEntry{"first error message", "unknown file", -1, "\"unknown function\""};
        auto expectedSecondEntry = common::Error::ErrorTraceEntry{"second error message", "test_Error.cpp", 27, "functionThatThrowsFromStdException"};

        CHECK_EQUAL(error.getErrorTrace().size(), 2);
        CHECK(error.getErrorTrace()[0] == expectedFirstEntry);
        CHECK(error.getErrorTrace()[1] == expectedSecondEntry);
    }
}

SARUS_UNITTEST_MAIN_FUNCTION();
