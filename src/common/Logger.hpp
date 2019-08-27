/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _SarusLogger_hpp
#define _SarusLogger_hpp

#include <string>
#include <iostream>

#include <boost/format.hpp>

#include "common/Error.hpp"


namespace sarus {
namespace common {

enum LogLevel {DEBUG, INFO, WARN, ERROR, GENERAL};

class Logger {
public:
    static Logger& getInstance();

    void log(const std::string& message, const std::string& sysName, const common::LogLevel& LogLevel,
    		std::ostream& out_stream = std::cout, std::ostream& err_stream = std::cerr);
    void log(const boost::format& message, const std::string& sysName, const common::LogLevel& LogLevel,
    		std::ostream& out_stream = std::cout, std::ostream& err_stream = std::cerr);
    void logErrorTrace(const common::Error& error, const std::string& sysName, std::ostream& errStream = std::cerr);
    void setLevel(common::LogLevel logLevel) { level = logLevel; };
    common::LogLevel getLevel() { return level; };

private:
    Logger();
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;

    std::string makeSubmessageWithTimestamp(common::LogLevel LogLevel) const;
    std::string makeSubmessageWithSarusInstanceID(common::LogLevel LogLevel) const;
    std::string makeSubmessageWithSystemName(   common::LogLevel LogLevel,
                                                const std::string& systemName) const;
    std::string makeSubmessageWithLogLevel(common::LogLevel LogLevel) const;

private:
    common::LogLevel level;
};

}
}

#endif
