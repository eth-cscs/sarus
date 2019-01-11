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

    CommandLoad(const std::deque<common::CLIArguments>& argsGroups, std::shared_ptr<common::Config> conf)
    : conf{std::move(conf)}
    {
        initializeOptionsDescription();
        parseCommandArguments(argsGroups);
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
            .setUsage("sarus load [OPTIONS] file IMAGE[:TAG]")
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

    void parseCommandArguments(const std::deque<common::CLIArguments>& argsGroups) {
        cli::utility::printLog( boost::format("parsing CLI arguments of load command"), common::logType::DEBUG);

        // the load command arguments (load [options] <archive> <image>) are composed
        // by exactly three groups of arguments (load + archive + image)
        if(argsGroups.size() != 3) {
            SARUS_THROW_ERROR("failed to parse CLI arguments of load command (bad number of arguments provided)");
        }

        try {
            boost::program_options::variables_map values;
            boost::program_options::store(
                boost::program_options::command_line_parser(argsGroups[0].argc(), argsGroups[0].argv())
                        .options(optionsDescription)
                        .run(), values);
            boost::program_options::notify(values);

            parsePathOfArchiveToBeLoaded(argsGroups[1]);

            conf->imageID = cli::utility::parseImageID(argsGroups[2]);
            conf->imageID.server = "load";
            conf->useCentralizedRepository = values.count("centralized-repository");
            conf->directories.initialize(conf->useCentralizedRepository, *conf);
        }
        catch (std::exception& e) {
            SARUS_RETHROW_ERROR(e, "failed to parse CLI arguments of load command");
        }

        cli::utility::printLog(boost::format("successfully parsed CLI arguments"), common::logType::DEBUG);
    }

    void parsePathOfArchiveToBeLoaded(const common::CLIArguments& archiveArgs) {
        if(archiveArgs.argc() != 1) {
            SARUS_THROW_ERROR("failed to parse archive's path"
                                " (archive's path is expected to be a single token without options)")
        }

        try {
            conf->archivePath = boost::filesystem::absolute(archiveArgs.argv()[0]);
        } catch(const std::exception& e) {
            auto message = boost::format("failed to convert archive's path %s to absolute path") % archiveArgs.argv()[0];
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
