/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_CommandHooks_hpp
#define cli_CommandHooks_hpp

#include <iostream>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <string>
#include <unordered_map>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string/join.hpp>
#include <rapidjson/pointer.h>

#include "common/Config.hpp"
#include "libsarus/CLIArguments.hpp"
#include "cli/Utility.hpp"
#include "cli/Command.hpp"
#include "cli/HelpMessage.hpp"
#include "runtime/OCIHooksFactory.hpp"


namespace sarus {
namespace cli {

class CommandHooks : public Command {
public:
    CommandHooks() {
        initializeOptionsDescription();
    }

    CommandHooks(const libsarus::CLIArguments& args, std::shared_ptr<common::Config> conf)
        : conf{std::move(conf)}
    {
        initializeOptionsDescription();
        parseCommandArguments(args);
    }

    void execute() override {
        auto fieldGetters = std::unordered_map<std::string, field_getter_t>{};
        fieldGetters["NAME"] = [](const runtime::OCIHook& hook) {
            return hook.jsonFile.stem().string();
        };
        fieldGetters["PATH"] = [](const runtime::OCIHook& hook) {
            return hook.jsonHook["path"].GetString();
        };
        fieldGetters["STAGES"] = [](const runtime::OCIHook& hook) {
            return boost::join(hook.stages, ",");
        };
        fieldGetters["MPI TYPE"] = [this](const runtime::OCIHook& hook) {
            auto ret = std::string{};
            for(const auto& condition : hook.conditions) {
                auto annotationCondition = dynamic_cast<runtime::OCIHook::ConditionAnnotations*>(condition.get());
                if(annotationCondition != nullptr) {
                    for(const auto& annotation : annotationCondition->getAnnotations()) {
                        auto keyRegex = boost::regex{ std::get<0>(annotation) };
                        auto valueRegex = boost::regex{ std::get<1>(annotation) };
                        auto matches = boost::smatch{};
                        if(boost::regex_match(std::string{"com.hooks.mpi.type"}, matches, keyRegex)) {
                            ret = std::get<1>(annotation);
                        }
                        if(const rapidjson::Value* defaultMPIType = rapidjson::Pointer("/defaultMPIType").Get(conf->json)) {
                            if(boost::regex_match(std::string{defaultMPIType->GetString()}, matches, valueRegex)) {
                                ret += " (default)";
                            }
                        }
                    }
                }
            }
            return ret;
        };

        auto hooksDir = boost::filesystem::path{ conf->json["hooksDir"].GetString() };
        auto schemaFile = boost::filesystem::path{ conf->json["prefixDir"].GetString() } / "etc/hook.schema.json";
        auto hooks = runtime::OCIHooksFactory{}.createHooks(hooksDir, schemaFile);

        if(listMpiHooks) {
            auto format = makeMpiFormat(hooks, fieldGetters);
            printMpiHooks(hooks, fieldGetters, format);
        }
        else {
            auto format = makeFormat(hooks, fieldGetters);
            printHooks(hooks, fieldGetters, format);
        }
    }

    bool requiresRootPrivileges() const override {
        return false;
    }

    std::string getBriefDescription() const override {
        return "List configured hooks";
    }

    void printHelpMessage() const override {
        auto printer = cli::HelpMessage()
            .setUsage("sarus hooks [OPTIONS]")
            .setDescription(getBriefDescription())
            .setOptionsDescription(optionsDescription);
        std::cout << printer;
    }

private:
    using field_getter_t = std::function<std::string(const runtime::OCIHook&)>;

private:
    void initializeOptionsDescription() {
        optionsDescription.add_options()
            ("mpi,m", "Only list MPI hooks and display corresponding MPI types");
    }

    void parseCommandArguments(const libsarus::CLIArguments& args) {
        cli::utility::printLog(boost::format("parsing CLI arguments of hooks command"), libsarus::LogLevel::DEBUG);

        libsarus::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = cli::utility::groupOptionsAndPositionalArguments(args, optionsDescription);

        // the hooks command doesn't support positional arguments
        cli::utility::validateNumberOfPositionalArguments(positionalArgs, 0, 0, "hooks");

        try {
            boost::program_options::variables_map values;
            boost::program_options::store(
                boost::program_options::command_line_parser(nameAndOptionArgs.argc(), nameAndOptionArgs.argv())
                        .options(optionsDescription)
                        .style(boost::program_options::command_line_style::unix_style)
                        .run(), values);
            boost::program_options::notify(values);

            if(values.count("mpi")) {
                listMpiHooks = true;
            }
        }
        catch(std::exception& e) {
            auto message = boost::format("%s\nSee 'sarus help hooks'") % e.what();
            utility::printLog(message, libsarus::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
        }

        cli::utility::printLog( boost::format("successfully parsed CLI arguments"), libsarus::LogLevel::DEBUG);
    }

    boost::format makeFormat(const std::vector<runtime::OCIHook>& hooks,
                             std::unordered_map<std::string, field_getter_t>& fieldGetters) const {
        auto formatString = std::string{};

        const std::size_t minFieldWidth = 10;
        std::size_t width;

        width = maxFieldLength(hooks, fieldGetters["NAME"]);
        width = std::max(width, minFieldWidth);
        formatString += "%-" + std::to_string(width) + "." + std::to_string(width) + "s";

        width = maxFieldLength(hooks, fieldGetters["PATH"]);
        width = std::max(width, minFieldWidth);
        formatString += "   %-" + std::to_string(width) + "." + std::to_string(width) + "s";

        formatString += "   %-s";

        return boost::format{formatString};
    }

    boost::format makeMpiFormat(const std::vector<runtime::OCIHook>& hooks,
                             std::unordered_map<std::string, field_getter_t>& fieldGetters) const {
        auto formatString = std::string{};

        const std::size_t minFieldWidth = 10;
        std::size_t width;

        width = maxFieldLength(hooks, fieldGetters["NAME"]);
        width = std::max(width, minFieldWidth);
        formatString += "%-" + std::to_string(width) + "." + std::to_string(width) + "s";

        formatString += "   %-s";

        return boost::format{formatString};
    }

    std::size_t maxFieldLength(const std::vector<runtime::OCIHook>& hooks,
                               const field_getter_t getField) const {
        std::size_t maxLength = 0;
        for(const auto& hook : hooks) {
            maxLength = std::max(maxLength, getField(hook).size());
        }
        return maxLength;
    }

    void printHooks(const std::vector<runtime::OCIHook>& hooks,
                    std::unordered_map<std::string, field_getter_t>& fieldGetters,
                    boost::format format) const {
        std::cout << format % "NAME" % "PATH" % "STAGES" << std::endl;

        for(const auto& hook : hooks) {
            // For some weird reason we need to create a copy of the strings retrieved through
            // the field getters. That's why below the std::string's copy constructor is used.
            std::cout   << format
                        % std::string(fieldGetters["NAME"](hook))
                        % std::string(fieldGetters["PATH"](hook))
                        % std::string(fieldGetters["STAGES"](hook))
                        << std::endl;
        }
    }

    void printMpiHooks(const std::vector<runtime::OCIHook>& hooks,
                       std::unordered_map<std::string, field_getter_t>& fieldGetters,
                       boost::format format) const {
        std::cout << format % "NAME" % "MPI TYPE" << std::endl;

        for(const auto& hook : hooks) {
            // For some weird reason we need to create a copy of the strings retrieved through
            // the field getters. That's why below the std::string's copy constructor is used.
            if(isMpiHook(hook)) {
                std::cout   << format
                            % std::string(fieldGetters["NAME"](hook))
                            % std::string(fieldGetters["MPI TYPE"](hook))
                            << std::endl;
            }
        }
    }

    bool isMpiHook(const runtime::OCIHook& hook) const {
        for(const auto& condition : hook.conditions) {
            auto annotationCondition = dynamic_cast<runtime::OCIHook::ConditionAnnotations*>(condition.get());
            if(annotationCondition != nullptr) {
                for(const auto& annotation : annotationCondition->getAnnotations()) {
                    auto keyRegex = boost::regex{ std::get<0>(annotation) };
                    auto valueRegex = boost::regex{ std::get<1>(annotation) };
                    auto matches = boost::smatch{};
                    if(boost::regex_match(std::string{"com.hooks.mpi.enabled"}, matches, keyRegex) &&
                       boost::regex_match(std::string{"true"}, matches, valueRegex)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

private:
    boost::program_options::options_description optionsDescription{"Options"};
    std::shared_ptr<common::Config> conf;
    bool listMpiHooks = false;
};

} // namespace
} // namespace

#endif
