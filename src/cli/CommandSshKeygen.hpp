/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_cli_CommandSshKeygen_hpp
#define sarus_cli_CommandSshKeygen_hpp

#include <memory>

#include "libsarus/Logger.hpp"
#include "libsarus/CLIArguments.hpp"
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

    CommandSshKeygen(const libsarus::CLIArguments& args, std::shared_ptr<common::Config> config)
        : conf{std::move(config)}
    {
        initializeOptionsDescription();
        parseCommandArguments(args);
        conf->useCentralizedRepository = false;
        conf->directories.initialize(conf->useCentralizedRepository, *conf);
    }

    void execute() override {
        libsarus::setEnvironmentVariable("HOOK_BASE_DIR", conf->json["localRepositoryBaseDir"].GetString());
        auto passwdFile = boost::filesystem::path{ conf->json["prefixDir"].GetString() } / "etc/passwd";
        libsarus::setEnvironmentVariable("PASSWD_FILE", passwdFile.string());
        auto dropbearDir = boost::filesystem::path{ conf->json["prefixDir"].GetString() } / "dropbear";
        libsarus::setEnvironmentVariable("DROPBEAR_DIR", dropbearDir.string());

        auto sshHook = boost::filesystem::path{conf->json["prefixDir"].GetString()} / "bin/ssh_hook";
        auto args = libsarus::CLIArguments{sshHook.string(), "keygen"};
        if(overwriteSshKeysIfExist) {
            args.push_back("--overwrite");
        }
        if(libsarus::Logger::getInstance().getLevel() == libsarus::LogLevel::INFO) {
            args.push_back("--verbose");
        }
        else if(libsarus::Logger::getInstance().getLevel() == libsarus::LogLevel::DEBUG) {
            args.push_back("--debug");
        }
        libsarus::forkExecWait(args);
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

    void parseCommandArguments(const libsarus::CLIArguments& args) {
        cli::utility::printLog(boost::format("parsing CLI arguments of ssh-keygen command"), libsarus::LogLevel::DEBUG);

        libsarus::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = cli::utility::groupOptionsAndPositionalArguments(args, optionsDescription);

        // the ssh-keygen command doesn't support positional arguments
        cli::utility::validateNumberOfPositionalArguments(positionalArgs, 0, 0, "ssh-keygen");

        try {
            boost::program_options::variables_map values;
            boost::program_options::store(
                boost::program_options::command_line_parser(nameAndOptionArgs.argc(), nameAndOptionArgs.argv())
                        .options(optionsDescription)
                        .style(boost::program_options::command_line_style::unix_style)
                        .run(), values);
            boost::program_options::notify(values);

            if(values.count("overwrite")) {
                overwriteSshKeysIfExist = true;
            }

        }
        catch(std::exception& e) {
            auto message = boost::format("%s\nSee 'sarus help ssh-keygen'") % e.what();
            utility::printLog(message, libsarus::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
        }

        cli::utility::printLog(boost::format("successfully parsed CLI arguments"), libsarus::LogLevel::DEBUG);
    }

private:
    boost::program_options::options_description optionsDescription{"Options"};
    std::shared_ptr<common::Config> conf;
    bool overwriteSshKeysIfExist = false;
};

}
}

#endif
