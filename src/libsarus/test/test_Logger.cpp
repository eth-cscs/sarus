/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <boost/regex.hpp>

#include "aux/unitTestMain.hpp"
#include "libsarus/Logger.hpp"


namespace libsarus {
namespace test {

TEST_GROUP(LoggerTestGroup) {
};

class LoggerChecker {
public:
    LoggerChecker& log(libsarus::LogLevel logLevel, const std::string& message) {
        auto& logger = libsarus::Logger::getInstance();
        logger.log(message, "subsystem", logLevel, stdoutStream, stderrStream);
        return *this;
    }

    LoggerChecker& expectGeneralMessageInStdout(const std::string& message) {
        auto pattern = ".*^" + message + "\n.*";
        expectedPatternInStdout += pattern;
        return *this;
    }

    LoggerChecker& expectMessageInStdout(const std::string& logLevel, const std::string& message) {
        return expectMessage(logLevel, message, expectedPatternInStdout);
    }

    LoggerChecker& expectMessageInStderr(const std::string& logLevel, const std::string& message) {
        return expectMessage(logLevel, message, expectedPatternInStderr);
    }

    ~LoggerChecker() {
        check(stdoutStream, expectedPatternInStdout);
        check(stderrStream, expectedPatternInStderr);
    }

private:
    LoggerChecker& expectMessage(const std::string& logLevel, const std::string& message, std::string& expectedPattern) {
        auto messagePattern = "\\[.*\\..*\\] \\[.*\\] \\[subsystem\\] \\[" + logLevel + "\\] " + message + "\n";
        expectedPattern += messagePattern;
        return *this;
    }

    void check(const std::ostringstream& stream, const std::string& expectedPattern) const {
        auto regex = boost::regex(expectedPattern);
        boost::cmatch matches;
        CHECK(boost::regex_match(stream.str().c_str(), matches, regex));
    }

private:
    std::ostringstream stdoutStream;
    std::ostringstream stderrStream;

    std::string expectedPatternInStdout;
    std::string expectedPatternInStderr;
};

TEST(LoggerTestGroup, logger) {
    const std::string generalMessage = "GENERAL message";
    const std::string debugMessage = "DEBUG message";
    const std::string infoMessage = "INFO message";
    const std::string warnMessage = "WARN message";
    const std::string errorMessage = "ERROR message";

    // DEBUG level
    libsarus::Logger::getInstance().setLevel(libsarus::LogLevel::DEBUG);
    LoggerChecker{}
        .log(libsarus::LogLevel::GENERAL, generalMessage)
        .log(libsarus::LogLevel::DEBUG, debugMessage)
        .log(libsarus::LogLevel::INFO, infoMessage)
        .log(libsarus::LogLevel::WARN, warnMessage)
        .log(libsarus::LogLevel::ERROR, errorMessage)
        .expectGeneralMessageInStdout(generalMessage)
        .expectMessageInStdout("DEBUG", debugMessage)
        .expectMessageInStdout("INFO", infoMessage)
        .expectMessageInStderr("WARN", warnMessage)
        .expectMessageInStderr("ERROR", errorMessage);

    // INFO level
    libsarus::Logger::getInstance().setLevel(libsarus::LogLevel::INFO);
    LoggerChecker{}
        .log(libsarus::LogLevel::GENERAL, generalMessage)
        .log(libsarus::LogLevel::DEBUG, debugMessage)
        .log(libsarus::LogLevel::INFO, infoMessage)
        .log(libsarus::LogLevel::WARN, warnMessage)
        .log(libsarus::LogLevel::ERROR, errorMessage)
        .expectGeneralMessageInStdout(generalMessage)
        .expectMessageInStdout("INFO", infoMessage)
        .expectMessageInStderr("WARN", warnMessage)
        .expectMessageInStderr("ERROR", errorMessage);

    // WARN level
    libsarus::Logger::getInstance().setLevel(libsarus::LogLevel::WARN);
    LoggerChecker{}
        .log(libsarus::LogLevel::GENERAL, generalMessage)
        .log(libsarus::LogLevel::DEBUG, debugMessage)
        .log(libsarus::LogLevel::INFO, infoMessage)
        .log(libsarus::LogLevel::WARN, warnMessage)
        .log(libsarus::LogLevel::ERROR, errorMessage)
        .expectGeneralMessageInStdout(generalMessage)
        .expectMessageInStderr("WARN", warnMessage)
        .expectMessageInStderr("ERROR", errorMessage);

    // ERROR level
    libsarus::Logger::getInstance().setLevel(libsarus::LogLevel::ERROR);
    LoggerChecker{}
        .log(libsarus::LogLevel::GENERAL, generalMessage)
        .log(libsarus::LogLevel::DEBUG, debugMessage)
        .log(libsarus::LogLevel::INFO, infoMessage)
        .log(libsarus::LogLevel::WARN, warnMessage)
        .log(libsarus::LogLevel::ERROR, errorMessage)
        .expectGeneralMessageInStdout(generalMessage)
        .expectMessageInStderr("ERROR", errorMessage);
}

}}

SARUS_UNITTEST_MAIN_FUNCTION();
