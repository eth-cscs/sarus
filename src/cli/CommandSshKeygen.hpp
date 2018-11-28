#ifndef sarus_cli_CommandSshKeygen_hpp
#define sarus_cli_CommandSshKeygen_hpp

#include "common/Logger.hpp"
#include "common/CLIArguments.hpp"
#include "cli/Command.hpp"
#include "cli/Utility.hpp"
#include "cli/HelpMessage.hpp"


namespace sarus {
namespace cli {

class CommandSshKeygen : public Command {
public:
    CommandSshKeygen() = default;

    CommandSshKeygen(const std::deque<common::CLIArguments>& argsGroups, const common::Config& conf)
        : Command(conf)
    {
        parseCommandArguments(argsGroups);
    }

    void execute() override {
        common::setEnvironmentVariable("SARUS_LOCAL_REPOSITORY_DIR="
            + common::getLocalRepositoryDirectory(conf).string());
        common::setEnvironmentVariable("SARUS_OPENSSH_DIR="
            + (conf.buildTime.prefixDir / "openssh").string());
        auto command = boost::format("%s/bin/ssh_hook keygen")
            % conf.buildTime.prefixDir.string();
        common::executeCommand(command.str());
        common::Logger::getInstance().log("Successfully generated SSH keys", "CLI", common::logType::GENERAL);
    }

    bool requiresRootPrivileges() const override {
        return false;
    }

    std::string getBriefDescription() const override {
        return "Generate the SSH keys in the local repository";
    }

    void printHelpMessage() const override {
        auto printer = cli::HelpMessage()
            .setUsage("sarus ssh-keygen")
            .setDescription(getBriefDescription());
        std::cout << printer;
    }

private:
    void parseCommandArguments(const std::deque<common::CLIArguments>& argsGroups) {
        cli::utility::printLog(boost::format("parsing CLI arguments of ssh-keygen command"), common::logType::DEBUG);

        // the ssh-keygen command arguments are composed by exactly one group of arguments
        if(argsGroups.size() > 1) {
            SARUS_THROW_ERROR("failed to parse CLI arguments of ssh-keygen command (too many arguments provided)");
        }

        conf.useCentralizedRepository = false;
        conf.directories.initialize(conf.useCentralizedRepository, conf);

        cli::utility::printLog(boost::format("successfully parsed CLI arguments"), common::logType::DEBUG);
    }
};

}
}

#endif
