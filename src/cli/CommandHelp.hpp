/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_CommandHelp_hpp
#define cli_CommandHelp_hpp

#include <iostream>
#include <algorithm>

#include "common/Config.hpp"
#include "cli/Utility.hpp"
#include "cli/Command.hpp"
#include "cli/CLI.hpp"
#include "cli/HelpMessage.hpp"
#include "cli/CommandObjectsFactory.hpp"

namespace sarus {
namespace cli {

class CommandHelp : public Command {
public:
    CommandHelp() = default;

    CommandHelp(const std::deque<common::CLIArguments>& argsGroups, std::shared_ptr<common::Config>) {
        if(!argsGroups.empty() && argsGroups[0].argc() > 1) {
            auto message = boost::format("Command 'help' doesn't support options");
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }
    }

    void execute() override {
        std::cout
        << "Usage: sarus COMMAND\n"
        << "\n"
        << cli::CLI{}.getOptionsDescription()
        << "\n"
        << "Commands:\n";

        printCommands();

        std::cout << "\nRun 'sarus help COMMAND' for more information about a command" << std::endl;
    }

    bool requiresRootPrivileges() const override {
        return false;
    }

    std::string getBriefDescription() const override {
        return "Print help message about a command";
    }

    void printHelpMessage() const override {
        auto printer = cli::HelpMessage()
            .setUsage("sarus help [COMMAND]")
            .setDescription(getBriefDescription());
        std::cout << printer;
    }

private:
    void printCommands() const {
        auto factory = CommandObjectsFactory{};
        auto commandNames = factory.getCommandNames();
        if(commandNames.empty()) {
            return;
        }
        std::sort(commandNames.begin(), commandNames.end()); // print commands in alphabetical order
        
        // make format
        auto it = std::max_element(commandNames.cbegin(), commandNames.cend(),
            [](const std::string& a, const std::string& b){
                return a.size() < b.size();
            });
        auto maxLength = it->size();
        auto format = boost::format("   %-" + std::to_string(maxLength + 3) + "." + std::to_string(maxLength + 3) + "s%s");

        // print commands
        for(const auto& name : commandNames) {
            auto description = factory.makeCommandObject(name)->getBriefDescription();
            std::cout << format % name % description << std::endl;
        }
    }
};

}
}

#endif
