/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <exception>

#include "aux/unitTestMain.hpp"
#include "libsarus/Error.hpp"


namespace libsarus {
namespace test {

TEST_GROUP(ErrorTestGroup) {
};

void functionThatThrows() {
    SARUS_THROW_ERROR("first error message");
}

void functionThatRethrows() {
    try {
        functionThatThrows();
    }
    catch(libsarus::Error& error) {
        SARUS_RETHROW_ERROR(error, "second error message");
    }
}

void functionThatThrowsFromStdException() {
    auto stdException = std::runtime_error("first error message");
    const auto& ref = stdException;
    SARUS_RETHROW_ERROR(ref, "second error message");
}

void functionThatThrowsWithLogLevelDebug() {
    SARUS_THROW_ERROR("first error message", libsarus::LogLevel::DEBUG);
}

void functionThatRethrowsWithLogLevelDebug() {
    try {
        functionThatThrows();
    }
    catch(libsarus::Error& error) {
        SARUS_RETHROW_ERROR(error, "second error message", libsarus::LogLevel::DEBUG);
    }
}

TEST(ErrorTestGroup, oneStackTraceEntry) {
    try {
        functionThatThrows();
    }
    catch(const libsarus::Error& error) {
        auto expectedFirstEntry = libsarus::Error::ErrorTraceEntry{"first error message", "test_Error.cpp", 24, "functionThatThrows"};

        CHECK_EQUAL(error.getErrorTrace().size(), 1);
        CHECK(error.getErrorTrace()[0] == expectedFirstEntry);
        CHECK(error.getLogLevel() == libsarus::LogLevel::ERROR);
    }
}

TEST(ErrorTestGroup, twoStackTraceEntries) {
    try {
        functionThatRethrows();
    }
    catch (const libsarus::Error& error) {
        auto expectedFirstEntry = libsarus::Error::ErrorTraceEntry{"first error message", "test_Error.cpp", 24, "functionThatThrows"};
        auto expectedSecondEntry = libsarus::Error::ErrorTraceEntry{"second error message", "test_Error.cpp", 32, "functionThatRethrows"};

        CHECK_EQUAL(error.getErrorTrace().size(), 2);
        CHECK(error.getErrorTrace()[0] == expectedFirstEntry);
        CHECK(error.getErrorTrace()[1] == expectedSecondEntry);
        CHECK(error.getLogLevel() == libsarus::LogLevel::ERROR);
    }
}

TEST(ErrorTestGroup, fromStdException) {
    try {
        functionThatThrowsFromStdException();
    }
    catch(const libsarus::Error& error) {
        auto expectedFirstEntry = libsarus::Error::ErrorTraceEntry{"first error message", "unspecified location", -1, "runtime error"};
        auto expectedSecondEntry = libsarus::Error::ErrorTraceEntry{"second error message", "test_Error.cpp", 39, "functionThatThrowsFromStdException"};

        CHECK_EQUAL(error.getErrorTrace().size(), 2);
        CHECK(error.getErrorTrace()[0] == expectedFirstEntry);
        CHECK(error.getErrorTrace()[1] == expectedSecondEntry);
        CHECK(error.getLogLevel() == libsarus::LogLevel::ERROR);
    }
}

TEST(ErrorTestGroup, oneStackTraceEntry_throwWithLogLevelDebug) {
    try {
        functionThatThrowsWithLogLevelDebug();
    }
    catch(const libsarus::Error& error) {
        auto expectedFirstEntry = libsarus::Error::ErrorTraceEntry{"first error message", "test_Error.cpp", 43, "functionThatThrowsWithLogLevelDebug"};

        CHECK_EQUAL(error.getErrorTrace().size(), 1);
        CHECK(error.getErrorTrace()[0] == expectedFirstEntry);
        CHECK(error.getLogLevel() == libsarus::LogLevel::DEBUG);
    }
}

TEST(ErrorTestGroup, twoStackTraceEntries_rethrowWithLogLevelDebug) {
    try {
        functionThatRethrowsWithLogLevelDebug();
    }
    catch (const libsarus::Error& error) {
        auto expectedFirstEntry = libsarus::Error::ErrorTraceEntry{"first error message", "test_Error.cpp", 24, "functionThatThrows"};
        auto expectedSecondEntry = libsarus::Error::ErrorTraceEntry{"second error message", "test_Error.cpp", 51, "functionThatRethrowsWithLogLevelDebug"};

        CHECK_EQUAL(error.getErrorTrace().size(), 2);
        CHECK(error.getErrorTrace()[0] == expectedFirstEntry);
        CHECK(error.getErrorTrace()[1] == expectedSecondEntry);
        CHECK(error.getLogLevel() == libsarus::LogLevel::DEBUG);
    }
}

}}

SARUS_UNITTEST_MAIN_FUNCTION();
