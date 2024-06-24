/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
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

#include "libsarus/Logger.hpp"
#include "libsarus/Error.hpp"
#include "common/Config.hpp"
#include "libsarus/CLIArguments.hpp"

namespace sarus {
namespace cli {
namespace utility {

bool isValidCLIInputImageReference(const std::string&);

common::ImageReference parseImageReference(const libsarus::CLIArguments& imageArgs);

common::ImageReference parseImageReference(const std::string& input);

std::tuple<libsarus::CLIArguments, libsarus::CLIArguments> groupOptionsAndPositionalArguments(
        const libsarus::CLIArguments&,
        const boost::program_options::options_description& optionsDescription);

void validateNumberOfPositionalArguments(const libsarus::CLIArguments& positionalArgs,
        const int min, const int max, const std::string& command);

void printLog(  const std::string& message, libsarus::LogLevel LogLevel,
                std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr);

void printLog(  const boost::format& message, libsarus::LogLevel LogLevel,
                std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr);

}
}
}

#endif
