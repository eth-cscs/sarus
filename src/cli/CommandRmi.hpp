/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_CommandRmi_hpp
#define cli_CommandRmi_hpp

#include <iostream>
#include <stdexcept>

#include <boost/format.hpp>

#include "cli/Utility.hpp"
#include "common/Config.hpp"
#include "cli/Command.hpp"
#include "common/CLIArguments.hpp"
#include "cli/HelpMessage.hpp"
#include "image_manager/ImageManager.hpp"


namespace sarus {
namespace cli {

class CommandRmi : public Command {
public:
    CommandRmi() {
        initializeOptionsDescription();
    }

    CommandRmi(const std::deque<common::CLIArguments>& argsGroups, std::shared_ptr<common::Config> conf)
        : conf{std::move(conf)}
    {
        initializeOptionsDescription();
        parseCommandArguments(argsGroups);
    }

    void execute() override {
        auto imageManager = image_manager::ImageManager{conf};
        imageManager.removeImage();
        // TODO: print to stdout (image manager shouldn't do it for the sake of testability)
    }

    bool requiresRootPrivileges() const override {
        return false;
    }

    std::string getBriefDescription() const override {
        return  "Remove an image";
    }

    void printHelpMessage() const override {
        auto printer = cli::HelpMessage()
            .setUsage("sarus rmi REPOSITORY[:TAG]\n"
                "\n"
                "Note: REPOSITORY[:TAG] has to be specified as\n"
                "      displayed by the \"sarus images\" command.")
            .setDescription(getBriefDescription());
        std::cout << printer;
    }

private:
    void initializeOptionsDescription() {
        optionsDescription.add_options()
            ("centralized-repository", "Use centralized repository instead of the local one");
    }

    void parseCommandArguments(const std::deque<common::CLIArguments>& argsGroups) {
        cli::utility::printLog(boost::format("parsing CLI arguments of rmi command"), common::LogLevel::DEBUG);

        // the rmi command expects exactly two arguments
        if(argsGroups.size() != 2) {
            auto message = boost::format("Bad number of arguments for command 'rmi'"
                                        "\nSee 'sarus help rmi'");
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::DEBUG);
        }

        try {
            boost::program_options::variables_map values;
            boost::program_options::store(
                boost::program_options::command_line_parser(argsGroups[0].argc(), argsGroups[0].argv())
                        .options(optionsDescription)
                        .run(), values);
            boost::program_options::notify(values);

            conf->useCentralizedRepository = values.count("centralized-repository");
        }
        catch (std::exception& e) {
            auto message = boost::format("%s\nSee 'sarus help rmi'") % e.what();
            cli::utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::DEBUG);
        }

        conf->directories.initialize(conf->useCentralizedRepository, *conf);
        conf->imageID = cli::utility::parseImageID(argsGroups[1]);

        cli::utility::printLog(boost::format("successfully parsed CLI arguments"), common::LogLevel::DEBUG);
    }

private:
    boost::program_options::options_description optionsDescription{"Options"};
    std::shared_ptr<common::Config> conf;
};

}
}

#endif
