/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/Logger.hpp"

#include <string>
#include <fstream>
#include <iostream>
#include <cerrno>

#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/Utility.hpp"


namespace sarus {
namespace common {

    Logger& Logger::getInstance() {
        static Logger logger;
        return logger;
    }

    Logger::Logger()
        : level{ common::logType::WARN }
    {}

    void Logger::log(const std::string& message, const std::string& systemName, const common::logType& logType,
    		std::ostream& out_stream, std::ostream& err_stream) {
        if(logType < level) {
            return;
        }

        auto fullLogMessage = makeSubmessageWithTimestamp(logType)
            + makeSubmessageWithSarusInstanceID(logType)
            + makeSubmessageWithSystemName(logType, systemName)
            + makeSubmessageWithLogLevel(logType)
            + message;

        // WARNING and ERROR messages go to stderr
        if ( logType == common::logType::WARN || logType == common::logType::ERROR ) {
            err_stream << fullLogMessage << std::endl;
        }
        // rest goes to stdout
        else {
            out_stream << fullLogMessage << std::endl;
        }
    }

    void Logger::log(const boost::format& message, const std::string& systemName, const common::logType& logType,
    		std::ostream& out_stream, std::ostream& err_stream) {
        log(message.str(), systemName, logType, out_stream, err_stream);
    }

    void Logger::logErrorTrace( const common::Error& error, const std::string& systemName, std::ostream& errStream) {
        log("Error trace (most nested error last):", systemName, logType::ERROR, std::cout, errStream);

        const auto& trace = error.getErrorTrace();
        for(size_t i=0; i!=trace.size(); ++i) {
            const auto& entry = trace[trace.size()-i-1];
            auto line = boost::format("#%-3.3s %s at %s:%s %s\n")
                % i % entry.functionName % entry.fileName % entry.fileLine % entry.errorMessage;
            errStream << line;
        }
    }

    std::string Logger::makeSubmessageWithTimestamp(common::logType logType) const {
        if(logType == common::logType::GENERAL) {
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

    std::string Logger::makeSubmessageWithSarusInstanceID(common::logType logType) const {
        if(logType == common::logType::GENERAL) {
            return "";
        }

        auto id = boost::format("[%s-%d] ") % common::getHostname() % getpid();
        return id.str();
    }

    std::string Logger::makeSubmessageWithSystemName(common::logType logType, const std::string& systemName) const {
        if(logType == common::logType::GENERAL) {
            return "";
        }

        return "[" + systemName + "] ";
    }

    std::string Logger::makeSubmessageWithLogLevel(common::logType logType) const {
        switch(logType) {
            case common::logType::DEBUG:   return "[DEBUG] ";
            case common::logType::INFO :   return "[INFO] ";
            case common::logType::WARN :   return "[WARN] ";
            case common::logType::ERROR:   return "[ERROR] ";
            case common::logType::GENERAL: return "";
        }
        SARUS_THROW_ERROR("logger failed to convert unknown log level to string");
    }

} // namespace
} // namespace
