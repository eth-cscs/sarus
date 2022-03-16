/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_CommandImages_hpp
#define cli_CommandImages_hpp

#include <iostream>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <string>
#include <unordered_map>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "../common/ImageReference.hpp"
#include "common/Config.hpp"
#include "common/CLIArguments.hpp"
#include "common/SarusImage.hpp"
#include "cli/Utility.hpp"
#include "cli/Command.hpp"
#include "cli/HelpMessage.hpp"
#include "image_manager/ImageManager.hpp"


namespace sarus {
namespace cli {

class CommandImages : public Command {
public:
    CommandImages() {
        initializeOptionsDescription();
    }

    CommandImages(const common::CLIArguments& args, std::shared_ptr<common::Config> conf)
        : conf{std::move(conf)}
    {
        initializeOptionsDescription();
        parseCommandArguments(args);
    }

    void execute() override {
        auto fieldGetters = std::unordered_map<std::string, field_getter_t>{};
        fieldGetters["SERVER"] = [](const common::SarusImage& image) {
            return image.imageReference.server;
        };
        fieldGetters["REPOSITORY"] = [](const common::SarusImage& image) {
            if(image.imageReference.server != common::ImageReference::DEFAULT_SERVER) {
                return image.imageReference.server
                    + "/" + image.imageReference.repositoryNamespace
                    + "/" + image.imageReference.image;
            }
            else if(image.imageReference.repositoryNamespace != common::ImageReference::DEFAULT_REPOSITORY_NAMESPACE) {
                return image.imageReference.repositoryNamespace
                    + "/" + image.imageReference.image;
            }
            else {
                return image.imageReference.image;
            }
        };
        fieldGetters["TAG"] = [](const common::SarusImage& image) {
            return image.imageReference.tag;
        };
        fieldGetters["DIGEST"] = [](const common::SarusImage& image) {
            return image.digest.empty() ? std::string{"<none>"} : image.digest;
        };
        fieldGetters["CREATED"] = [](const common::SarusImage& image) {
            return image.created;
        };
        fieldGetters["SIZE"] = [](const common::SarusImage& image) {
            return image.datasize;
        };

        auto imageManager = image_manager::ImageManager{conf};
        auto images = imageManager.listImages();
        auto format = makeFormat(images, fieldGetters);
        printImages(images, fieldGetters, format);
    }

    bool requiresRootPrivileges() const override {
        return false;
    }

    std::string getBriefDescription() const override {
        return "List images";
    }

    void printHelpMessage() const override {
        auto printer = cli::HelpMessage()
            .setUsage("sarus images")
            .setDescription(getBriefDescription())
            .setOptionsDescription(optionsDescription);
        std::cout << printer;
    }

private:
    using field_getter_t = std::function<std::string(const common::SarusImage&)>;

private:
    void initializeOptionsDescription() {
        optionsDescription.add_options()
            ("centralized-repository", "Use centralized repository instead of the local one");
    }

    void parseCommandArguments(const common::CLIArguments& args) {
        cli::utility::printLog(boost::format("parsing CLI arguments of images command"), common::LogLevel::DEBUG);

        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = cli::utility::groupOptionsAndPositionalArguments(args, optionsDescription);

        // the images command doesn't support positional arguments
        cli::utility::validateNumberOfPositionalArguments(positionalArgs, 0, 0, "images");

        try {
            boost::program_options::variables_map values;
            boost::program_options::store(
                boost::program_options::command_line_parser(nameAndOptionArgs.argc(), nameAndOptionArgs.argv())
                        .options(optionsDescription)
                        .style(boost::program_options::command_line_style::unix_style)
                        .run(), values);
            boost::program_options::notify(values);

            conf->useCentralizedRepository = values.count("centralized-repository");
            conf->directories.initialize(conf->useCentralizedRepository, *conf);
        }
        catch(std::exception& e) {
            auto message = boost::format("%s\nSee 'sarus help images'") % e.what();
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }

        cli::utility::printLog( boost::format("successfully parsed CLI arguments"), common::LogLevel::DEBUG);
    }

    boost::format makeFormat(   const std::vector<common::SarusImage>& images,
                                std::unordered_map<std::string, field_getter_t>& fieldGetters) const {
        auto formatString = std::string{};

        const std::size_t minFieldWidth = 10;
        std::size_t width;

        width = maxFieldLength(images, fieldGetters["REPOSITORY"]);
        width = std::max(width, minFieldWidth);
        formatString += "%-" + std::to_string(width) + "." + std::to_string(width) + "s";

        width = maxFieldLength(images, fieldGetters["TAG"]);
        width = std::max(width, minFieldWidth);
        formatString += "   %-" + std::to_string(width) + "." + std::to_string(width) + "s";

        std::size_t printedCharactersOfDigest = 12;
        formatString += "   %-" + std::to_string(printedCharactersOfDigest)
            + "." + std::to_string(printedCharactersOfDigest) + "s";

        width = maxFieldLength(images, fieldGetters["CREATED"]);
        width = std::max(width, minFieldWidth);
        formatString += "   %-" + std::to_string(width) + "." + std::to_string(width) + "s";

        width = maxFieldLength(images, fieldGetters["SIZE"]);
        width = std::max(width, minFieldWidth);
        formatString += "   %-" + std::to_string(width) + "." + std::to_string(width) + "s";

        formatString += "   %-s";

        return boost::format{formatString};
    }

    std::size_t maxFieldLength( const std::vector<common::SarusImage>& images,
                                const field_getter_t getField) const {
        std::size_t maxLength = 0;
        for(const auto& image : images) {
            maxLength = std::max(maxLength, getField(image).size());
        }
        return maxLength;
    }

    void printImages(   const std::vector<common::SarusImage>& images,
                        std::unordered_map<std::string, field_getter_t>& fieldGetters,
                        boost::format format) {
        std::cout << format % "REPOSITORY" % "TAG" % "DIGEST" % "CREATED" % "SIZE" % "SERVER\n";

        for(const auto& image : images) {
            // For some weird reason we need to create a copy of the strings retrieved through
            // the field getters. That's why below the std::string's copy constructor is used.
            std::cout   << format
                        % std::string(fieldGetters["REPOSITORY"](image))
                        % std::string(fieldGetters["TAG"](image))
                        % std::string(fieldGetters["DIGEST"](image))
                        % std::string(fieldGetters["CREATED"](image))
                        % std::string(fieldGetters["SIZE"](image))
                        % std::string(fieldGetters["SERVER"](image))
                        << std::endl;
        }
    }

private:
    boost::program_options::options_description optionsDescription{"Options"};
    std::shared_ptr<common::Config> conf;
    std::unique_ptr<image_manager::ImageManager> imageManager;
};

} // namespace
} // namespace

#endif
