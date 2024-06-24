/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_Error_hpp
#define libsarus_Error_hpp

#include <type_traits>
#include <exception>
#include <string>
#include <vector>
#include <cstring>

#include <boost/filesystem.hpp>

#include "libsarus/LogLevel.hpp"

namespace libsarus {

/**
 * This class encapsulates error trace information to be propagated as an exception.
 * 
 * An error trace entry encapsulates information about file, line and function name
 * where the error trace entry was created.
 * 
 * The first error trace entry is created by the macro SARUS_THROW_ERROR.
 * Additional error trace entries are created by the macro SARUS_RETHROW_ERROR.
 * 
 * Note: this class should be instantiated and thrown through the SARUS_THROW_ERROR macro.
 * Caught instances of this class should be rethrown through the SARUS_RETHROW_ERROR macro.
 * The user is not supposed to instantiate and throw this class "manually".
 */
class Error : public std::exception {
public:
    struct ErrorTraceEntry {
        std::string errorMessage;
        boost::filesystem::path fileName;
        int fileLine;
        std::string functionName;
    };

public:
    Error(LogLevel logLevel, const ErrorTraceEntry& entry)
        : logLevel{ logLevel }
        , errorTrace{ entry }
    {}

    const char* what() const noexcept override {
        // Return the 'what()' of the original exception that generated this error trace
        // as if the original exception was propagated directly up to the current
        // stack frame, i.e. without intermediate catch-rethrows.
        return errorTrace.front().errorMessage.c_str();
    }

    void appendErrorTraceEntry(const ErrorTraceEntry& entry) {
        errorTrace.push_back(entry);
    }

    const std::vector<ErrorTraceEntry>& getErrorTrace() const {
        return errorTrace;
    }

    LogLevel getLogLevel() const {
        return logLevel;
    }

    void setLogLevel(LogLevel value) {
        logLevel = value;
    }

private:
    LogLevel logLevel = LogLevel::ERROR;
    std::vector<ErrorTraceEntry> errorTrace;
};

inline bool operator==(const Error::ErrorTraceEntry& lhs, const Error::ErrorTraceEntry& rhs) {
    return lhs.errorMessage == rhs.errorMessage
        && lhs.fileName == rhs.fileName
        && lhs.fileLine == rhs.fileLine
        && lhs.functionName == rhs.functionName;
}

inline bool operator!=(const Error::ErrorTraceEntry& lhs, const Error::ErrorTraceEntry& rhs) {
    return !(lhs == rhs);
}

std::string getExceptionTypeString(const std::exception& e);

}


// SARUS_THROW_ERROR macros
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define SARUS_GET_OVERLOADED_THROW_ERROR(_1, _2, NAME, ...) NAME

#define SARUS_THROW_ERROR_2(errorMessage, logLevel) { \
    auto stackTraceEntry = libsarus::Error::ErrorTraceEntry{errorMessage, __FILENAME__, __LINE__, __func__}; \
    throw libsarus::Error{logLevel, stackTraceEntry}; \
}

#define SARUS_THROW_ERROR_1(errorMessage) SARUS_THROW_ERROR_2(errorMessage, libsarus::LogLevel::ERROR)

#define SARUS_THROW_ERROR(...) SARUS_GET_OVERLOADED_THROW_ERROR(__VA_ARGS__, SARUS_THROW_ERROR_2, SARUS_THROW_ERROR_1)(__VA_ARGS__)


// SARUS_RETHROW_ERROR macros
#define SARUS_GET_OVERLOADED_RETHROW_ERROR(_1, _2, _3, NAME, ...) NAME

#define SARUS_RETHROW_ERROR_3(exception, errorMessage, logLevel) { \
    auto errorTraceEntry = libsarus::Error::ErrorTraceEntry{errorMessage, __FILENAME__, __LINE__, __func__}; \
    const auto* cp = dynamic_cast<const libsarus::Error*>(&exception); \
    if(cp) { /* check if dynamic type is libsarus::Error */ \
        assert(!std::is_const<decltype(exception)>{}); /* a libsarus::Error object must be caught as non-const reference because we need to modify its internal error trace */ \
        auto* p = const_cast<libsarus::Error*>(cp); \
        p->setLogLevel(logLevel); \
        p->appendErrorTraceEntry(errorTraceEntry); \
        throw; \
    } \
    else { \
        auto previousErrorTraceEntry = libsarus::Error::ErrorTraceEntry{exception.what(), "unspecified location", -1, \
                                                                             libsarus::getExceptionTypeString(exception)}; \
        auto error = libsarus::Error{logLevel, previousErrorTraceEntry}; \
        error.appendErrorTraceEntry(errorTraceEntry); \
        throw error; \
    } \
}

#define SARUS_RETHROW_ERROR_2(exception, errorMessage) { \
    const auto* cp = dynamic_cast<const libsarus::Error*>(&exception); \
    if(cp) { \
        /* get log level if dynamic type is libsarus::Error */ \
        SARUS_RETHROW_ERROR_3(exception, errorMessage, cp->getLogLevel()) \
    } \
    else { \
        SARUS_RETHROW_ERROR_3(exception, errorMessage, libsarus::LogLevel::ERROR) \
    } \
}

#define SARUS_RETHROW_ERROR(...) SARUS_GET_OVERLOADED_RETHROW_ERROR(__VA_ARGS__, SARUS_RETHROW_ERROR_3, SARUS_RETHROW_ERROR_2)(__VA_ARGS__)

#endif
