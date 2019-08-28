/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_cli_CommandSshKeygen_hpp
#define sarus_cli_CommandSshKeygen_hpp

#include <memory>

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

    CommandSshKeygen(const std::deque<common::CLIArguments>& argsGroups, std::shared_ptr<common::Config> config)
        : conf{std::move(config)}
    {
        parseCommandArguments(argsGroups);
        conf->useCentralizedRepository = false;
        conf->directories.initialize(conf->useCentralizedRepository, *conf);
    }

    void execute() override {
        common::setEnvironmentVariable("SARUS_LOCAL_REPOSITORY_DIR="
            + common::getLocalRepositoryDirectory(*conf).string());
        common::setEnvironmentVariable("SARUS_OPENSSH_DIR="
            + (conf->buildTime.prefixDir / "openssh").string());
        auto command = boost::format("%s/bin/ssh_hook keygen")
            % conf->buildTime.prefixDir.string();
        common::executeCommand(command.str());
        common::Logger::getInstance().log("Successfully generated SSH keys", "CLI", common::LogLevel::GENERAL);
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
        cli::utility::printLog(boost::format("parsing CLI arguments of ssh-keygen command"), common::LogLevel::DEBUG);

        // the ssh-keygen command doesn't support additional arguments
        if(argsGroups.size() > 1) {
            auto message = boost::format("Command 'ssh-keygen' doesn't support additional arguments"
                                         "\nSee 'sarus help ssh-keygen'");
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::DEBUG);
        }

        if(argsGroups[0].argc() > 1) {
            auto message = boost::format("Command 'ssh-keygen' doesn't support options"
                                         "\nSee 'sarus help ssh-keygen'");
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::DEBUG);
        }

        cli::utility::printLog(boost::format("successfully parsed CLI arguments"), common::LogLevel::DEBUG);
    }

private:
    std::shared_ptr<common::Config> conf;
};

}
}

#endif
