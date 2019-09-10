/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
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
    auto argsGroups = groupArgumentsAndCorrespondingOptions(args);
    boost::program_options::variables_map values;
    auto factory = cli::CommandObjectsFactory{};
    auto& logger = common::Logger::getInstance();

    try {
        boost::program_options::store(
            boost::program_options::command_line_parser(argsGroups.front().argc(), argsGroups.front().argv())
                .options(optionsDescription)
                .run(), values);
        boost::program_options::notify(values); // throw if options are invalid
    }
    catch (const std::exception& e) {
        auto message = boost::format("Error while parsing the command line: %s") % e.what();
        logger.log(message, "CLI", common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::DEBUG);
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

    // drop first argument
    // (string "sarus" and the related general options that we have already parsed)
    argsGroups.pop_front();

    // --help option
    if(values.count("help")) {
        argsGroups.clear(); // --help overrides other arguments and options
        return factory.makeCommandObject("help", argsGroups, std::move(conf));
    }

    // --version option
    if(values.count("version")) {
        argsGroups.clear(); // --version overrides other arguments and options
        return factory.makeCommandObject("version", argsGroups, std::move(conf));
    }

    // no command name => return help command
    if(argsGroups.empty()) {
        return factory.makeCommandObject("help");
    }

    auto commandName = std::string{argsGroups.front().argv()[0]};

    // process help command of another command
    bool isCommandHelpFollowedByAnArgument = commandName == "help" && argsGroups.size() > 1;
    if(isCommandHelpFollowedByAnArgument) {
        return parseCommandHelpOfCommand(argsGroups);
    }

    return factory.makeCommandObject(commandName, argsGroups, std::move(conf));
}

const boost::program_options::options_description& CLI::getOptionsDescription() const {
    return optionsDescription;
}


/**
 * Group the arguments and their options into individual CLIArguments objects.
 * 
 * E.g. the CLI arguments "sarus --verbose run image command" is broken down into
 * four CLIArguments objects ("sarus --verbose", "run", "image", "command").
 */
std::deque<common::CLIArguments> CLI::groupArgumentsAndCorrespondingOptions(const common::CLIArguments& args) const {
    auto isOption = [](char* s) {
        bool hasDashPrefix = strlen(s) > 1 && s[0]=='-' && s[1]!='-';
        bool hasDashDashPrefix = strlen(s) > 2 && s[0]=='-' && s[1]=='-' && s[2]!='-';
        return hasDashPrefix || hasDashDashPrefix;
    };

    assert(args.argc() >= 1);
    assert(!isOption(args.argv()[0]));

    auto result = std::deque<common::CLIArguments>{ };

    for(const auto arg : args) {
        if(!isOption(arg)) {
            result.push_back(common::CLIArguments{});
        }
        result.back().push_back(arg);
    }

    return result;
}

std::unique_ptr<cli::Command> CLI::parseCommandHelpOfCommand(const std::deque<common::CLIArguments>& argsGroups) const {
    if(argsGroups[0].argc() > 1) {
        auto message = boost::format("Command 'help' doesn't support options");
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::DEBUG);
    }
    if(argsGroups.size() > 2) {
        auto message = boost::format("Command 'help' doesn't support extra argument '%s'"
                                     "\nSee 'sarus help help'") % argsGroups[2].argv()[0];
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::DEBUG);
    }
    auto factory = cli::CommandObjectsFactory{};
    auto commandName = std::string{ argsGroups[1].argv()[0] };
    return factory.makeCommandObjectHelpOfCommand(commandName);
}

} // namespace
} // namespace
