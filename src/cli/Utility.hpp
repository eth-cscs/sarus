/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_Utility_hpp
#define cli_Utility_hpp

#include <string>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include "common/Logger.hpp"
#include "common/Error.hpp"
#include "common/Config.hpp"
#include "common/CLIArguments.hpp"

namespace sarus {
namespace cli {
namespace utility {

bool isValidCLIInputImageID(const std::string&);

common::ImageID parseImageID(const common::CLIArguments& imageArgs);

common::ImageID parseImageID(const std::string& input);

void printLog(  const std::string& message, common::logType logType,
                std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr);

void printLog(  const boost::format& message, common::logType logType,
                std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr);

}
}
}

#endif
