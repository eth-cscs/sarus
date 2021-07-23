/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
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

    CommandRmi(const common::CLIArguments& args, std::shared_ptr<common::Config> conf)
        : conf{std::move(conf)}
    {
        initializeOptionsDescription();
        parseCommandArguments(args);
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

    void parseCommandArguments(const common::CLIArguments& args) {
        cli::utility::printLog(boost::format("parsing CLI arguments of rmi command"), common::LogLevel::DEBUG);

        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = cli::utility::groupOptionsAndPositionalArguments(args, optionsDescription);

        // the rmi command expects exactly one positional argument
        cli::utility::validateNumberOfPositionalArguments(positionalArgs, 1, 1, "rmi");

        try {
            boost::program_options::variables_map values;
            boost::program_options::store(
                boost::program_options::command_line_parser(nameAndOptionArgs.argc(), nameAndOptionArgs.argv())
                        .options(optionsDescription)
                        .style(boost::program_options::command_line_style::unix_style)
                        .run(), values);
            boost::program_options::notify(values);

            conf->imageID = cli::utility::parseImageID(positionalArgs.argv()[0]);
            conf->useCentralizedRepository = values.count("centralized-repository");
            conf->directories.initialize(conf->useCentralizedRepository, *conf);
        }
        catch (std::exception& e) {
            auto message = boost::format("%s\nSee 'sarus help rmi'") % e.what();
            cli::utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }

        cli::utility::printLog(boost::format("successfully parsed CLI arguments"), common::LogLevel::DEBUG);
    }

private:
    boost::program_options::options_description optionsDescription{"Options"};
    std::shared_ptr<common::Config> conf;
};

}
}

#endif
