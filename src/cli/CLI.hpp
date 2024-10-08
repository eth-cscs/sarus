/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_CLI_hpp
#define cli_CLI_hpp

#include <memory>
#include <deque>

#include <boost/program_options.hpp>
#include <boost/format.hpp>

#include "libsarus/Logger.hpp"
#include "cli/Utility.hpp"
#include "common/Config.hpp"
#include "cli/Command.hpp"
#include "libsarus/CLIArguments.hpp"


namespace sarus {
namespace cli {

class CLI {
public:
    CLI();
    std::unique_ptr<cli::Command> parseCommandLine(const libsarus::CLIArguments&, std::shared_ptr<common::Config>) const;

// these methods are public for test purpose
public:
    const boost::program_options::options_description& getOptionsDescription() const;

private:
    std::unique_ptr<cli::Command> parseCommandHelpOfCommand(const libsarus::CLIArguments&) const;

private:
    boost::program_options::options_description optionsDescription{"Options"};
};

}
}

#endif
