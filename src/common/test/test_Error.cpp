/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
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

void functionThatThrowsWithLogLevelDebug() {
    SARUS_THROW_ERROR("first error message", common::LogLevel::DEBUG);
}

void functionThatRethrowsWithLogLevelDebug() {
    try {
        functionThatThrows();
    }
    catch(common::Error& error) {
        SARUS_RETHROW_ERROR(error, "second error message", common::LogLevel::DEBUG);
    }
}

TEST(ErrorTestGroup, oneStackTraceEntry) {
    try {
        functionThatThrows();
    }
    catch(const common::Error& error) {
        auto expectedFirstEntry = common::Error::ErrorTraceEntry{"first error message", "test_Error.cpp", 22, "functionThatThrows"};

        CHECK_EQUAL(error.getErrorTrace().size(), 1);
        CHECK(error.getErrorTrace()[0] == expectedFirstEntry);
        CHECK(error.getLogLevel() == common::LogLevel::ERROR);
    }
}

TEST(ErrorTestGroup, twoStackTraceEntries) {
    try {
        functionThatRethrows();
    }
    catch (const common::Error& error) {
        auto expectedFirstEntry = common::Error::ErrorTraceEntry{"first error message", "test_Error.cpp", 22, "functionThatThrows"};
        auto expectedSecondEntry = common::Error::ErrorTraceEntry{"second error message", "test_Error.cpp", 30, "functionThatRethrows"};

        CHECK_EQUAL(error.getErrorTrace().size(), 2);
        CHECK(error.getErrorTrace()[0] == expectedFirstEntry);
        CHECK(error.getErrorTrace()[1] == expectedSecondEntry);
        CHECK(error.getLogLevel() == common::LogLevel::ERROR);
    }
}

TEST(ErrorTestGroup, fromStdException) {
    try {
        functionThatThrowsFromStdException();
    }
    catch(const common::Error& error) {
        auto expectedFirstEntry = common::Error::ErrorTraceEntry{"first error message", "unspecified location", -1, "runtime error"};
        auto expectedSecondEntry = common::Error::ErrorTraceEntry{"second error message", "test_Error.cpp", 37, "functionThatThrowsFromStdException"};

        CHECK_EQUAL(error.getErrorTrace().size(), 2);
        CHECK(error.getErrorTrace()[0] == expectedFirstEntry);
        CHECK(error.getErrorTrace()[1] == expectedSecondEntry);
        CHECK(error.getLogLevel() == common::LogLevel::ERROR);
    }
}

TEST(ErrorTestGroup, oneStackTraceEntry_throwWithLogLevelDebug) {
    try {
        functionThatThrowsWithLogLevelDebug();
    }
    catch(const common::Error& error) {
        auto expectedFirstEntry = common::Error::ErrorTraceEntry{"first error message", "test_Error.cpp", 41, "functionThatThrowsWithLogLevelDebug"};

        CHECK_EQUAL(error.getErrorTrace().size(), 1);
        CHECK(error.getErrorTrace()[0] == expectedFirstEntry);
        CHECK(error.getLogLevel() == common::LogLevel::DEBUG);
    }
}

TEST(ErrorTestGroup, twoStackTraceEntries_rethrowWithLogLevelDebug) {
    try {
        functionThatRethrowsWithLogLevelDebug();
    }
    catch (const common::Error& error) {
        auto expectedFirstEntry = common::Error::ErrorTraceEntry{"first error message", "test_Error.cpp", 22, "functionThatThrows"};
        auto expectedSecondEntry = common::Error::ErrorTraceEntry{"second error message", "test_Error.cpp", 49, "functionThatRethrowsWithLogLevelDebug"};

        CHECK_EQUAL(error.getErrorTrace().size(), 2);
        CHECK(error.getErrorTrace()[0] == expectedFirstEntry);
        CHECK(error.getErrorTrace()[1] == expectedSecondEntry);
        CHECK(error.getLogLevel() == common::LogLevel::DEBUG);
    }
}

SARUS_UNITTEST_MAIN_FUNCTION();
