#ifndef sarus_common_Error_hpp
#define sarus_common_Error_hpp

#include <type_traits>
#include <exception>
#include <string>
#include <vector>
#include <string.h>

#include <boost/filesystem.hpp>


namespace sarus {
namespace common {

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
    Error(const ErrorTraceEntry& entry)
        : errorTrace{ entry }
    {}

    void appendErrorTraceEntry(const ErrorTraceEntry& entry) {
        errorTrace.push_back(entry);
    }

    const std::vector<ErrorTraceEntry>& getErrorTrace() const {
        return errorTrace;
    }

private:
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

} // namespace
} // namespace

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define SARUS_THROW_ERROR(errorMessage) { \
    auto stackTraceEntry = sarus::common::Error::ErrorTraceEntry{errorMessage, __FILENAME__, __LINE__, __func__}; \
    throw sarus::common::Error{stackTraceEntry}; \
}

#define SARUS_RETHROW_ERROR(stdException, errorMessage) { \
    auto errorTraceEntry = sarus::common::Error::ErrorTraceEntry{errorMessage, __FILENAME__, __LINE__, __func__}; \
    const auto* cp = dynamic_cast<const sarus::common::Error*>(&stdException); \
    if(cp) { /* check if dynamic type is common::Error */ \
        assert(!std::is_const<decltype(stdException)>{}); /* a common::Error object must be caucht as non-const reference because we need to modify its internal error trace */ \
        auto* p = const_cast<sarus::common::Error*>(cp); \
        p->appendErrorTraceEntry(errorTraceEntry); \
        throw; \
    } \
    else { \
        auto previousErrorTraceEntry = sarus::common::Error::ErrorTraceEntry{stdException.what(), "unknown file", -1, "\"unknown function\""}; \
        auto error = sarus::common::Error{previousErrorTraceEntry}; \
        error.appendErrorTraceEntry(errorTraceEntry); \
        throw error; \
    } \
}

#endif
