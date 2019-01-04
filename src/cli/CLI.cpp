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

    try {
        boost::program_options::store(
            boost::program_options::command_line_parser(argsGroups.front().argc(), argsGroups.front().argv())
                .options(optionsDescription)
                .allow_unregistered()
                .run(), values);
        boost::program_options::notify(values); // throw if options are invalid
    }
    catch (const std::exception& e) {
        SARUS_RETHROW_ERROR(e, "failed to parse CLI arguments");
    }

    if(values.count("help")) {
        return factory.makeCommandObject("help");
    }

    if(values.count("version")) {
        return factory.makeCommandObject("version");
    }

    // configure logger
    auto& logger = common::Logger::getInstance();
    if(values.count("debug")) {
        logger.setLevel(common::logType::DEBUG);
    }
    else if(values.count("verbose")) {
        logger.setLevel(common::logType::INFO);
    }
    else {
        logger.setLevel(common::logType::WARN);
    }

    // no command name => return help command
    if(argsGroups.size() == 1) {
        return factory.makeCommandObject("help");
    }

    auto commandName = std::string{argsGroups[1].argv()[0]};

    // process help command of another command
    bool isCommandHelpFollowedByAnArgument =
        commandName == "help" && argsGroups.size() > 2;
    if(isCommandHelpFollowedByAnArgument) {
        return parseCommandHelpOfCommand(argsGroups);
    }

    // drop first argument
    // (string "sarus" and the related general options that we have already parsed)
    argsGroups.pop_front();

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
    auto factory = cli::CommandObjectsFactory{};
    auto commandName = std::string{ argsGroups[2].argv()[0] };
    if(!factory.isValidCommandName(commandName)) {
        auto message = boost::format("unknown help topic: %s") % commandName;
        SARUS_THROW_ERROR(message.str());
    }
    return factory.makeCommandObjectHelpOfCommand(commandName);
}

} // namespace
} // namespace
