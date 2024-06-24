/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
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

#include "libsarus/Utility.hpp"
#include "cli/Utility.hpp"
#include "common/Config.hpp"
#include "cli/Command.hpp"
#include "libsarus/CLIArguments.hpp"
#include "cli/HelpMessage.hpp"
#include "image_manager/ImageManager.hpp"


namespace sarus {
namespace cli {

class CommandPull : public Command {
public:
    CommandPull() {
        initializeOptionsDescription();
    }

    CommandPull(const libsarus::CLIArguments& args, std::shared_ptr<common::Config> conf)
        : conf{std::move(conf)}
    {
        initializeOptionsDescription();
        parseCommandArguments(args);
    }

    void execute() override {
        auto imageManager = image_manager::ImageManager{conf};
        imageManager.pullImage(transport);
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
            .setOptionsDescription(visibleOptionsDescription);
        std::cout << printer;
    }

private:
    void initializeOptionsDescription() {
        visibleOptionsDescription.add_options()
            ("temp-dir",
                boost::program_options::value<std::string>(&conf->directories.tempFromCLI),
                "Temporary directory where the image is unpacked")
            ("login", "Enter user credentials for private repository from stdin. "
                      "Cannot be used in conjunction with '--password-stdin'")
            ("password-stdin", "Read password for private repository from stdin. "
                      "Cannot be used in conjunction with '--login'")
            ("username,u",
                boost::program_options::value<std::string>(&username),
                "Username for private repository")
            ("centralized-repository", "Use centralized repository instead of the local one");
        hiddenOptionsDescription.add_options()
            ("containers-storage", "Pull from a local containers/storage image store");
        allOptionsDescription.add(visibleOptionsDescription).add(hiddenOptionsDescription);
    }

    void parseCommandArguments(const libsarus::CLIArguments& args) {
        cli::utility::printLog(boost::format("parsing CLI arguments of pull command"), libsarus::LogLevel::DEBUG);

        libsarus::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = cli::utility::groupOptionsAndPositionalArguments(args, allOptionsDescription);

        // the pull command expects exactly one positional argument
        cli::utility::validateNumberOfPositionalArguments(positionalArgs, 1, 1, "pull");

        try {
            boost::program_options::variables_map values;
            boost::program_options::store(
                boost::program_options::command_line_parser(nameAndOptionArgs.argc(), nameAndOptionArgs.argv())
                        .options(allOptionsDescription)
                        .style(boost::program_options::command_line_style::unix_style)
                        .run(), values);
            boost::program_options::notify(values);

            if(values.count("username")) {
                conf->authentication.isAuthenticationNeeded = true;
                validateUsername(username);
                conf->authentication.username = username;
            }

            if(values.count("password-stdin")) {
                if(values.count("login")) {
                    SARUS_THROW_ERROR("The options '--password-stdin' and '--login' cannot be used together");
                }
                conf->authentication.isAuthenticationNeeded = true;
                conf->authentication.password = readPasswordFromStdin();
            }

            if(values.count("login")) {
                conf->authentication.isAuthenticationNeeded = true;
                readUserCredentialsFromCLI(conf->authentication);
            }

            if(values.count("containers-storage")) {
                transport = "containers-storage";
            }
            else {
                transport = "docker";
            }

            conf->imageReference = cli::utility::parseImageReference(positionalArgs.argv()[0]);
            conf->useCentralizedRepository = values.count("centralized-repository");
            conf->directories.initialize(conf->useCentralizedRepository, *conf);
        }
        catch (std::exception& e) {
            auto message = boost::format("%s\nSee 'sarus help pull'") % e.what();
            cli::utility::printLog(message, libsarus::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
        }

        cli::utility::printLog(boost::format("successfully parsed CLI arguments"), libsarus::LogLevel::DEBUG);
    }

    /**
     * Get the username/password from user input, and store into config.
     */
    void readUserCredentialsFromCLI(common::Config::Authentication& authentication) {
        cli::utility::printLog(boost::format("reading user credentials from CLI"), libsarus::LogLevel::DEBUG);

        std::cout << "username: ";
        if (username.empty()) {
            std::getline(std::cin, username);

            validateUsername(username);
            authentication.username = username;
        }
        else {
            std::cout << username << std::endl;
        }

        std::cout << "password: ";
        authentication.password = readPasswordFromStdin();
        std::cout << std::endl;

        cli::utility::printLog(boost::format("successfully read user credentials"), libsarus::LogLevel::DEBUG);
    }

    void validateUsername(const std::string& username) {
        if (username.empty()) {
            auto message = std::string{"Invalid username: empty value provided"};
            SARUS_THROW_ERROR(message);
        }
    }

    std::string readPasswordFromStdin() {
        auto password = std::string{};

        libsarus::setStdinEcho(false);  // disable echo
        std::getline(std::cin, password);
        libsarus::setStdinEcho(true);   // enable echo

        if(password.empty()) {
            SARUS_THROW_ERROR("Failed to read password from stdin: empty value provided");
        }

        return password;
    }

private:
    boost::program_options::options_description allOptionsDescription{};
    boost::program_options::options_description visibleOptionsDescription{"Options"};
    boost::program_options::options_description hiddenOptionsDescription{};
    std::shared_ptr<common::Config> conf;
    std::string username;
    std::string transport;
};

}
}

#endif
