#ifndef _SarusLogger_hpp
#define _SarusLogger_hpp

#include <string>
#include <iostream>

#include <boost/format.hpp>

#include "common/Error.hpp"


namespace sarus {
namespace common {

enum logType {DEBUG, INFO, WARN, ERROR, GENERAL};

class Logger {
public:
    static Logger& getInstance();

    void log(const std::string& message, const std::string& sysName, const common::logType& logType,
    		std::ostream& out_stream = std::cout, std::ostream& err_stream = std::cerr);
    void log(const boost::format& message, const std::string& sysName, const common::logType& logType,
    		std::ostream& out_stream = std::cout, std::ostream& err_stream = std::cerr);
    void logErrorTrace(const common::Error& error, const std::string& sysName, std::ostream& errStream = std::cerr);
    void setLevel(common::logType logLevel) { level = logLevel; };
    common::logType getLevel() { return level; };

private:
    Logger();
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;

    std::string makeSubmessageWithTimestamp(common::logType logType) const;
    std::string makeSubmessageWithSarusInstanceID(common::logType logType) const;
    std::string makeSubmessageWithSystemName(   common::logType logType,
                                                const std::string& systemName) const;
    std::string makeSubmessageWithLogLevel(common::logType logType) const;

private:
    common::logType level;
};

}
}

#endif
