/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_CommandLoad_hpp
#define cli_CommandLoad_hpp

#include <iostream>
#include <stdexcept>
#include <string>

#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include "common/Logger.hpp"
#include "cli/Utility.hpp"
#include "common/Config.hpp"
#include "cli/Command.hpp"
#include "common/CLIArguments.hpp"
#include "cli/HelpMessage.hpp"
#include "image_manager/ImageManager.hpp"


namespace sarus {
namespace cli {

class CommandLoad : public Command {
public:
    CommandLoad() {
        initializeOptionsDescription();
    }

    CommandLoad(const common::CLIArguments& args, std::shared_ptr<common::Config> conf)
    : conf{std::move(conf)}
    {
        initializeOptionsDescription();
        parseCommandArguments(args);
    }

    void execute() override {
        auto imageManager = image_manager::ImageManager{conf};
        imageManager.loadImage(conf->archivePath);
    }

    bool requiresRootPrivileges() const override {
        return false;
    }

    std::string getBriefDescription() const override {
        return "Load the contents of a tarball to create a filesystem image";
    }

    void printHelpMessage() const override {
        auto printer = cli::HelpMessage()
            .setUsage("sarus load [OPTIONS] file REPOSITORY[:TAG]")
            .setDescription(getBriefDescription())
            .setOptionsDescription(optionsDescription);
        std::cout << printer;
    }

private:
    void initializeOptionsDescription() {
        optionsDescription.add_options()
            ("temp-dir",   boost::program_options::value<std::string>(&conf->directories.tempFromCLI),
                "Temporary directory where the image is expanded")
            ("centralized-repository", "Use centralized repository instead of the local one");
    }

    void parseCommandArguments(const common::CLIArguments& args) {
        cli::utility::printLog( boost::format("parsing CLI arguments of load command"), common::LogLevel::DEBUG);

        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = cli::utility::groupOptionsAndPositionalArguments(args, optionsDescription);

        // the load command expects exactly two positional arguments
        cli::utility::validateNumberOfPositionalArguments(positionalArgs, 2, 2, "load");

        try {
            boost::program_options::variables_map values;
            boost::program_options::store(
                boost::program_options::command_line_parser(nameAndOptionArgs.argc(), nameAndOptionArgs.argv())
                        .options(optionsDescription)
                        .style(boost::program_options::command_line_style::unix_style)
                        .run(), values);
            boost::program_options::notify(values);

            parsePathOfArchiveToBeLoaded(positionalArgs.argv()[0]);

            conf->imageID = cli::utility::parseImageID(positionalArgs.argv()[1]);
            conf->imageID.server = "load";
            conf->useCentralizedRepository = values.count("centralized-repository");
            conf->directories.initialize(conf->useCentralizedRepository, *conf);
        }
        catch (std::exception& e) {
            auto message = boost::format("%s\nSee 'sarus help load'") % e.what();
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }

        cli::utility::printLog(boost::format("successfully parsed CLI arguments"), common::LogLevel::DEBUG);
    }

    void parsePathOfArchiveToBeLoaded(const boost::filesystem::path& archiveArg) {
        try {
            conf->archivePath = boost::filesystem::absolute(archiveArg);
        } catch(const std::exception& e) {
            auto message = boost::format("failed to convert archive's path %s to absolute path") % archiveArg;
            SARUS_RETHROW_ERROR(e, message.str());
        }
    }

private:
    boost::program_options::options_description optionsDescription{"Options"};
    std::shared_ptr<common::Config> conf;
};

}
}

#endif
