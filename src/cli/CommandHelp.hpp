#ifndef cli_CommandHelp_hpp
#define cli_CommandHelp_hpp

#include <iostream>
#include <algorithm>

#include "common/Config.hpp"
#include "cli/Command.hpp"
#include "cli/CLI.hpp"
#include "cli/HelpMessage.hpp"
#include "cli/CommandObjectsFactory.hpp"

namespace sarus {
namespace cli {

class CommandHelp : public Command {
public:
    CommandHelp() = default;

    CommandHelp(const std::deque<common::CLIArguments>&, std::shared_ptr<common::Config>)
    {}

    void execute() override {
        std::cout
        << "Usage: sarus COMMAND\n"
        << "\n"
        << cli::CLI{}.getOptionsDescription()
        << "\n"
        << "Commands:\n";

        auto factory = CommandObjectsFactory{};
        auto commandNames = factory.getCommandNames();
        std::sort(commandNames.begin(), commandNames.end());
        for(const auto& name : commandNames) {
            auto description = factory.makeCommandObject(name)->getBriefDescription();
            std::cout << "   " << name << ": " << description << "\n";
        }
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
};

}
}

#endif
