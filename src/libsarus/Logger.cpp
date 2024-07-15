/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "libsarus/Logger.hpp"

#include <string>
#include <fstream>
#include <iostream>
#include <cerrno>

#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

#include <boost/format.hpp>

#include "libsarus/Error.hpp"
#include "libsarus/Utility.hpp"

namespace libsarus {

    Logger& Logger::getInstance() {
        static Logger logger;
        return logger;
    }

    Logger::Logger()
        : level{ libsarus::LogLevel::WARN }
    {}

    void Logger::log(const std::string& message, const std::string& systemName, const libsarus::LogLevel& logLevel,
    		std::ostream& out_stream, std::ostream& err_stream) {
        if(logLevel < level) {
            return;
        }

        auto fullLogMessage = makeSubmessageWithTimestamp(logLevel)
            + makeSubmessageWithSarusInstanceID(logLevel)
            + makeSubmessageWithSystemName(logLevel, systemName)
            + makeSubmessageWithLogLevel(logLevel)
            + message;

        // WARNING and ERROR messages go to stderr
        if ( logLevel == libsarus::LogLevel::WARN || logLevel == libsarus::LogLevel::ERROR ) {
            err_stream << fullLogMessage << std::endl;
        }
        // rest goes to stdout
        else {
            out_stream << fullLogMessage << std::endl;
        }
    }

    void Logger::log(const boost::format& message, const std::string& systemName, const libsarus::LogLevel& logLevel,
    		std::ostream& out_stream, std::ostream& err_stream) {
        log(message.str(), systemName, logLevel, out_stream, err_stream);
    }

    void Logger::logErrorTrace( const libsarus::Error& error, const std::string& systemName, std::ostream& errStream) {
        if(error.getLogLevel() < level) {
            return;
        }

        log("Error trace (most nested error last):", systemName, LogLevel::ERROR, std::cout, errStream);

        const auto& trace = error.getErrorTrace();
        for(size_t i=0; i!=trace.size(); ++i) {
            const auto& entry = trace[trace.size()-i-1];
            auto line = boost::format("#%-3.3s %s at %s:%s %s\n")
                % i % entry.functionName % entry.fileName % (entry.fileLine != -1 ? std::to_string(entry.fileLine) : "")
                % entry.errorMessage;
            errStream << line;
        }
    }

    std::string Logger::makeSubmessageWithTimestamp(libsarus::LogLevel logLevel) const {
        if(logLevel == libsarus::LogLevel::GENERAL) {
            return "";
        }

        auto tp = timespec{};
        if(clock_gettime(CLOCK_MONOTONIC, &tp) != 0) {
            auto message = boost::format("logger failed to retrieve UNIX epoch time (%s)") % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }

        auto timestamp = boost::format("[%d.%d] ") % tp.tv_sec % tp.tv_nsec;
        return timestamp.str();
    }

    std::string Logger::makeSubmessageWithSarusInstanceID(libsarus::LogLevel logLevel) const {
        if(logLevel == libsarus::LogLevel::GENERAL) {
            return "";
        }

        auto id = boost::format("[%s-%d] ") % libsarus::process::getHostname() % getpid();
        return id.str();
    }

    std::string Logger::makeSubmessageWithSystemName(libsarus::LogLevel logLevel, const std::string& systemName) const {
        if(logLevel == libsarus::LogLevel::GENERAL) {
            return "";
        }

        return "[" + systemName + "] ";
    }

    std::string Logger::makeSubmessageWithLogLevel(libsarus::LogLevel logLevel) const {
        switch(logLevel) {
            case libsarus::LogLevel::DEBUG:   return "[DEBUG] ";
            case libsarus::LogLevel::INFO :   return "[INFO] ";
            case libsarus::LogLevel::WARN :   return "[WARN] ";
            case libsarus::LogLevel::ERROR:   return "[ERROR] ";
            case libsarus::LogLevel::GENERAL: return "";
        }
        SARUS_THROW_ERROR("logger failed to convert unknown log level to string");
    }

}
