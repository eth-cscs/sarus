/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_CommandPull_hpp
#define cli_CommandPull_hpp

#include <iostream>
#include <stdexcept>
#include <string>

#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include "common/Utility.hpp"
#include "cli/Utility.hpp"
#include "common/Config.hpp"
#include "cli/Command.hpp"
#include "common/CLIArguments.hpp"
#include "cli/HelpMessage.hpp"
#include "image_manager/ImageManager.hpp"


namespace sarus {
namespace cli {

class CommandPull : public Command {
public:
    CommandPull() {
        initializeOptionsDescription();
    }

    CommandPull(const std::deque<common::CLIArguments>& argsGroups, std::shared_ptr<common::Config> conf)
        : conf{std::move(conf)}
    {
        initializeOptionsDescription();
        parseCommandArguments(argsGroups);
    }

    void execute() override {
        auto imageManager = image_manager::ImageManager{conf};
        imageManager.pullImage();
    }

    bool requiresRootPrivileges() const override {
        return false;
    }

    std::string getBriefDescription() const override {
        return "Pull an image from a registry";
    }

    void printHelpMessage() const override {
        auto printer = cli::HelpMessage()
            .setUsage("sarus pull [OPTIONS] REPOSITORY[:TAG]")
            .setDescription(getBriefDescription())
            .setOptionsDescription(optionsDescription);
        std::cout << printer;
    }

private:
    void initializeOptionsDescription() {
        optionsDescription.add_options()
            ("temp-dir",
                boost::program_options::value<std::string>(&conf->directories.tempFromCLI),
                "Temporary directory where the image is expanded")
            ("login", "Enter user credentials for private repository")
            ("centralized-repository", "Use centralized repository instead of the local one");
    }

    void parseCommandArguments(const std::deque<common::CLIArguments>& argsGroups) {
        cli::utility::printLog(boost::format("parsing CLI arguments of pull command"), common::LogLevel::DEBUG);

        // the pull command expects exactly two arguments
        if(argsGroups.size() != 2) {
            auto message = boost::format("Bad number of arguments for command 'pull'"
                                        "\nSee 'sarus help pull'");
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

            if(values.count("login")) {
                conf->authentication.isAuthenticationNeeded = true;
                readUserCredentialsFromCLI(conf->authentication);
            }

            conf->imageID = cli::utility::parseImageID(argsGroups[1]);
            conf->useCentralizedRepository = values.count("centralized-repository");
            conf->directories.initialize(conf->useCentralizedRepository, *conf);
        }
        catch (std::exception& e) {
            auto message = boost::format("%s\nSee 'sarus help pull'") % e.what();
            cli::utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }

        cli::utility::printLog(boost::format("successfully parsed CLI arguments"), common::LogLevel::DEBUG);
    }

    /**
     * Get the username/password from user input, and store into config.
     */
    static void readUserCredentialsFromCLI(common::Config::Authentication& authentication) {
        cli::utility::printLog(boost::format("reading user credentials from CLI"), common::LogLevel::DEBUG);

        std::cout << "username: ";
        std::getline(std::cin, authentication.username);

        if(authentication.username.empty()) {
            SARUS_THROW_ERROR("failed to read username from CLI (empty username is not valid)");
        }

        std::cout << "password: ";
        common::SetStdinEcho(false);  // disable echo
        std::getline(std::cin, authentication.password);
        common::SetStdinEcho(true);   // enable echo
        std::cout << std::endl;

        if(authentication.password.empty()) {
            SARUS_THROW_ERROR("failed to read user's password from CLI (empty password is not valid)");
        }

        cli::utility::printLog(boost::format("successfully read user credentials"), common::LogLevel::DEBUG);
    }

private:
    boost::program_options::options_description optionsDescription{"Options"};
    std::shared_ptr<common::Config> conf;
};

}
}

#endif
