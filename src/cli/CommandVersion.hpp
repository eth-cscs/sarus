/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
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
#include "common/CLIArguments.hpp"
#include "cli/HelpMessage.hpp"


namespace sarus {
namespace cli {

class CommandVersion : public Command {
public:
    CommandVersion() = default;

    CommandVersion(const std::deque<common::CLIArguments>& argsGroups, std::shared_ptr<const common::Config> conf)
        : conf{std::move(conf)}
    {
        parseCommandArguments(argsGroups);
    }

    void execute() override {
        common::Logger::getInstance().log(conf->buildTime.version, "CommandVersion", common::LogLevel::GENERAL);
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
    void parseCommandArguments(const std::deque<common::CLIArguments>& argsGroups) {
        cli::utility::printLog(boost::format("parsing CLI arguments of version command"), common::LogLevel::DEBUG);

        // the version command doesn't support additional arguments
        if(argsGroups.size() > 1) {
            auto message = boost::format("Command 'version' doesn't support extra argument '%s'"
                                         "\nSee 'sarus help version'") % argsGroups[1].argv()[0];
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::DEBUG);
        }
        // the version command doesn't support options
        if(!argsGroups.empty() && argsGroups[0].argc() > 1) {
            auto message = boost::format("Command 'version' doesn't support options"
                                         "\nSee 'sarus help version'");
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::DEBUG);
        }

        cli::utility::printLog(boost::format("successfully parsed CLI arguments"), common::LogLevel::DEBUG);
    }

private:
    std::shared_ptr<const common::Config> conf;
};

}
}

#endif
