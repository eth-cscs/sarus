#ifndef _runtime_Utility_hpp
#define _runtime_Utility_hpp

#include <boost/format.hpp>

#include "common/Logger.hpp"

namespace sarus {
namespace runtime {
namespace utility {

void logMessage(const boost::format&, common::logType);
void logMessage(const std::string&, common::logType);

} // namespace
} // namespace
} // namespace

#endif
