/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_CommandVersion_hpp
#define cli_CommandVersion_hpp

#include <iostream>
#include <memory>

#include "common/Config.hpp"
#include "cli/Command.hpp"
#include "libsarus/CLIArguments.hpp"
#include "cli/HelpMessage.hpp"


namespace sarus {
namespace cli {

class CommandVersion : public Command {
public:
    CommandVersion() = default;

    CommandVersion(const libsarus::CLIArguments& args, std::shared_ptr<const common::Config> conf)
        : conf{std::move(conf)}
    {
        parseCommandArguments(args);
    }

    void execute() override {
        libsarus::Logger::getInstance().log(conf->buildTime.version, "CommandVersion", libsarus::LogLevel::GENERAL);
    }

    bool requiresRootPrivileges() const override {
        return false;
    }

    std::string getBriefDescription() const override {
        return "Show the Sarus version information";
    }

    void printHelpMessage() const override {
        auto printer = cli::HelpMessage()
            .setUsage("sarus version")
            .setDescription(getBriefDescription());
        std::cout << printer;
    }

private:
    void parseCommandArguments(const libsarus::CLIArguments& args) {
        cli::utility::printLog(boost::format("parsing CLI arguments of version command"), libsarus::LogLevel::DEBUG);

        auto optionsDescription = boost::program_options::options_description();
        libsarus::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = cli::utility::groupOptionsAndPositionalArguments(args, optionsDescription);

        // the version command doesn't support positional arguments
        cli::utility::validateNumberOfPositionalArguments(positionalArgs, 0, 0, "version");

        // the version command doesn't support options
        if(nameAndOptionArgs.argc() > 1) {
            auto message = boost::format("Command 'version' doesn't support options"
                                         "\nSee 'sarus help version'");
            utility::printLog(message, libsarus::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
        }

        cli::utility::printLog(boost::format("successfully parsed CLI arguments"), libsarus::LogLevel::DEBUG);
    }

private:
    std::shared_ptr<const common::Config> conf;
};

}
}

#endif
