#ifndef cli_CommandVersion_hpp
#define cli_CommandVersion_hpp

#include <iostream>

#include "common/Config.hpp"
#include "cli/Command.hpp"
#include "common/CLIArguments.hpp"
#include "cli/HelpMessage.hpp"


namespace sarus {
namespace cli {

class CommandVersion : public Command {
public:
    CommandVersion() = default;

    CommandVersion(const std::deque<common::CLIArguments>&, const common::Config& conf)
        : Command(conf)
    {}

    void execute() override {
        common::Logger::getInstance().log(conf.buildTime.version, "CommandVersion", common::logType::GENERAL);
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
};

}
}

#endif
