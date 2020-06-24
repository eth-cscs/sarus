/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
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
    CommandSshKeygen() {
        initializeOptionsDescription();
    }

    CommandSshKeygen(const std::deque<common::CLIArguments>& argsGroups, std::shared_ptr<common::Config> config)
        : conf{std::move(config)}
    {
        initializeOptionsDescription();
        parseCommandArguments(argsGroups);
        conf->useCentralizedRepository = false;
        conf->directories.initialize(conf->useCentralizedRepository, *conf);
    }

    void execute() override {
        common::setEnvironmentVariable("HOOK_BASE_DIR=" + std::string{conf->json["localRepositoryBaseDir"].GetString()});
        auto passwdFile = boost::filesystem::path{ conf->json["prefixDir"].GetString() } / "etc/passwd";
        common::setEnvironmentVariable("PASSWD_FILE=" + passwdFile.string());
        auto dropbearDir = boost::filesystem::path{ conf->json["prefixDir"].GetString() } / "dropbear";
        common::setEnvironmentVariable("DROPBEAR_DIR=" + dropbearDir.string());

        auto sshHook = boost::filesystem::path{conf->json["prefixDir"].GetString()} / "bin/ssh_hook";
        auto args = common::CLIArguments{sshHook.string(), "keygen"};
        if(overwriteSshKeysIfExist) {
            args.push_back("--overwrite");
        }
        common::forkExecWait(args);
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
            .setDescription(getBriefDescription())
            .setOptionsDescription(optionsDescription);
        std::cout << printer;
    }

private:
    void initializeOptionsDescription() {
        optionsDescription.add_options()
            ("overwrite", "Overwrite the SSH keys if they already exist");
    }

    void parseCommandArguments(const std::deque<common::CLIArguments>& argsGroups) {
        cli::utility::printLog(boost::format("parsing CLI arguments of ssh-keygen command"), common::LogLevel::DEBUG);

        // the ssh-keygen command doesn't support additional arguments
        if(argsGroups.size() > 1) {
            auto message = boost::format("Bad number of arguments for command 'ssh-keygen'"
                                         "\nSee 'sarus help ssh-keygen'");
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }

        try {
            boost::program_options::variables_map values;
            boost::program_options::store(
                boost::program_options::command_line_parser(argsGroups[0].argc(), argsGroups[0].argv())
                        .options(optionsDescription)
                        .run(), values);
            boost::program_options::notify(values);

            if(values.count("overwrite")) {
                overwriteSshKeysIfExist = true;
            }

        }
        catch(std::exception& e) {
            auto message = boost::format("%s\nSee 'sarus help ssh-keygen'") % e.what();
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }

        cli::utility::printLog(boost::format("successfully parsed CLI arguments"), common::LogLevel::DEBUG);
    }

private:
    boost::program_options::options_description optionsDescription{"Options"};
    std::shared_ptr<common::Config> conf;
    bool overwriteSshKeysIfExist = false;
};

}
}

#endif
