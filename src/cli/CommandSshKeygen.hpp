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

    CommandSshKeygen(const std::deque<common::CLIArguments>& argsGroups, std::shared_ptr<common::Config> conf)
        : conf{std::move(conf)}
    {
        parseCommandArguments(argsGroups);
    }

    void execute() override {
        common::setEnvironmentVariable(std::string{"SARUS_PREFIX_DIR="} + conf->json["prefixDir"].GetString());
        common::setEnvironmentVariable("SARUS_OPENSSH_DIR="
            + (conf->json["prefixDir"].GetString() + std::string{"/openssh"}));
        auto command = boost::format("%s/bin/ssh_hook keygen")
            % conf->json["prefixDir"].GetString();
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

        conf->useCentralizedRepository = false;
        conf->directories.initialize(conf->useCentralizedRepository, *conf);

        cli::utility::printLog(boost::format("successfully parsed CLI arguments"), common::logType::DEBUG);
    }

private:
    std::shared_ptr<common::Config> conf;
};

}
}

#endif
