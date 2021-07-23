/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "CLI.hpp"

#include <cstdlib>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/Utility.hpp"
#include "cli/Utility.hpp"
#include "cli/CommandObjectsFactory.hpp"


namespace sarus {
namespace cli {

CLI::CLI() {
    optionsDescription.add_options()
        ("help", "Print help")
        ("version", "Print version information and quit")
        ("debug", "Enable debug mode (print all log messages with DEBUG level or higher)")
        ("verbose", "Enable verbose mode (print all log messages with INFO level or higher)");
}

std::unique_ptr<cli::Command> CLI::parseCommandLine(const common::CLIArguments& args, std::shared_ptr<common::Config> conf) const {
    common::CLIArguments nameAndOptionArgs, positionalArgs;
    std::tie(nameAndOptionArgs, positionalArgs) = cli::utility::groupOptionsAndPositionalArguments(args, optionsDescription);
    boost::program_options::variables_map values;
    auto factory = cli::CommandObjectsFactory{};
    auto& logger = common::Logger::getInstance();

    try {
        boost::program_options::store(
            boost::program_options::command_line_parser(nameAndOptionArgs.argc(), nameAndOptionArgs.argv())
                .options(optionsDescription)
                .style(boost::program_options::command_line_style::unix_style)
                .run(), values);
        boost::program_options::notify(values); // throw if options are invalid
    }
    catch (const std::exception& e) {
        auto message = boost::format("%s\nSee 'sarus help'") % e.what();
        logger.log(message, "CLI", common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }

    // configure logger
    if(values.count("debug")) {
        logger.setLevel(common::LogLevel::DEBUG);
    }
    else if(values.count("verbose")) {
        logger.setLevel(common::LogLevel::INFO);
    }
    else {
        logger.setLevel(common::LogLevel::WARN);
    }

    // --help option
    if(values.count("help")) {
        // --help overrides other arguments and options
        return factory.makeCommandObject("help", common::CLIArguments{}, std::move(conf));
    }

    // --version option
    if(values.count("version")) {
        // --version overrides other arguments and options
        return factory.makeCommandObject("version", common::CLIArguments{}, std::move(conf));
    }

    // no command name => return help command
    if(positionalArgs.empty()) {
        return factory.makeCommandObject("help");
    }

    auto commandName = std::string{positionalArgs.argv()[0]};

    // process help command of another command
    bool isCommandHelpFollowedByAnArgument = commandName == "help" && positionalArgs.argc() > 1;
    if(isCommandHelpFollowedByAnArgument) {
        return parseCommandHelpOfCommand(positionalArgs);
    }

    return factory.makeCommandObject(commandName, positionalArgs, std::move(conf));
}

const boost::program_options::options_description& CLI::getOptionsDescription() const {
    return optionsDescription;
}

std::unique_ptr<cli::Command> CLI::parseCommandHelpOfCommand(const common::CLIArguments& args) const {
    auto optionsDescription = boost::program_options::options_description();
    common::CLIArguments nameAndOptionArgs, positionalArgs;
    std::tie(nameAndOptionArgs, positionalArgs) = cli::utility::groupOptionsAndPositionalArguments(args, optionsDescription);
    if(nameAndOptionArgs.argc() > 1) {
        auto message = boost::format("Command 'help' doesn't support options");
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }
    if(positionalArgs.argc() > 1) {
        auto message = boost::format("Too many arguments for command 'help'"
                                     "\nSee 'sarus help help'");
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }
    auto factory = cli::CommandObjectsFactory{};
    auto commandName = std::string{ positionalArgs.argv()[0] };
    return factory.makeCommandObjectHelpOfCommand(commandName);
}

} // namespace
} // namespace
