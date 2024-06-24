/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_Logger_hpp
#define libsarus_Logger_hpp

#include <string>
#include <iostream>

#include <boost/format.hpp>

#include "libsarus/LogLevel.hpp"
#include "libsarus/Error.hpp"

namespace libsarus {

class Logger {
public:
    static Logger& getInstance();

    void log(const std::string& message, const std::string& sysName, const libsarus::LogLevel& logLevel,
    		std::ostream& out_stream = std::cout, std::ostream& err_stream = std::cerr);
    void log(const boost::format& message, const std::string& sysName, const libsarus::LogLevel& logLevel,
    		std::ostream& out_stream = std::cout, std::ostream& err_stream = std::cerr);
    void logErrorTrace(const libsarus::Error& error, const std::string& sysName, std::ostream& errStream = std::cerr);
    void setLevel(libsarus::LogLevel logLevel) { level = logLevel; };
    libsarus::LogLevel getLevel() { return level; };

private:
    Logger();
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;

    std::string makeSubmessageWithTimestamp(libsarus::LogLevel logLevel) const;
    std::string makeSubmessageWithSarusInstanceID(libsarus::LogLevel logLevel) const;
    std::string makeSubmessageWithSystemName(   libsarus::LogLevel logLevel,
                                                const std::string& systemName) const;
    std::string makeSubmessageWithLogLevel(libsarus::LogLevel logLevel) const;

private:
    libsarus::LogLevel level;
};

}

#endif
