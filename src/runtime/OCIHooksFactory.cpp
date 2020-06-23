/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "OCIHooksFactory.hpp"

#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/Utility.hpp"
#include "runtime/Utility.hpp"

namespace sarus {
namespace runtime {

std::vector<OCIHook> OCIHooksFactory::createHooks(const boost::filesystem::path& hooksDir,
                                                  const boost::filesystem::path& schemaFile) const {
    utility::logMessage(boost::format{"Creating OCI hooks from %s"} % hooksDir,
                        common::LogLevel::INFO);

    if(!boost::filesystem::is_directory(hooksDir)) {
        auto message = boost::format{"Specified hooks directory %s is not valid"} % hooksDir;
        SARUS_THROW_ERROR(message.str());
    }

    auto jsonFiles = std::vector<boost::filesystem::path>{};
    for(boost::filesystem::directory_iterator entry{hooksDir};
        entry != boost::filesystem::directory_iterator{};
        ++entry) {
        if(entry->path().extension() == ".json") {
            utility::logMessage(boost::format{"Found OCI hook's config file %s"} % entry->path(),
                                common::LogLevel::DEBUG);
            jsonFiles.push_back(entry->path());
        }
    }

    std::sort(jsonFiles.begin(), jsonFiles.end());

    auto hooks = std::vector<OCIHook>{};
    for(const auto& jsonFile : jsonFiles) {
        hooks.push_back(createHook(jsonFile, schemaFile));
    }

    utility::logMessage(boost::format{"Successfully created %d OCI hooks"} % hooks.size(),
                        common::LogLevel::INFO);

    return hooks;
}

OCIHook OCIHooksFactory::createHook(const boost::filesystem::path& jsonFile,
                                    const boost::filesystem::path& schemaFile) const {
    utility::logMessage(boost::format{"Creating OCI hook object from %s"} % jsonFile,
                        common::LogLevel::INFO);
    auto json = common::readAndValidateJSON(jsonFile, schemaFile);
    auto hook = OCIHook{};

    hook.jsonFile = jsonFile;

    if(std::string(json["version"].GetString()) != "1.0.0") {
        auto message = boost::format{
            "Failed to parse OCI hook. The only supported version"
            " is 1.0.0, but found %s."} % json["version"].GetString();
        SARUS_THROW_ERROR(message.str());
    }
    hook.version = json["version"].GetString();

    hook.jsonHook = rapidjson::Document{ rapidjson::kObjectType };
    hook.jsonHook.CopyFrom(json["hook"], hook.jsonHook.GetAllocator());

    for(const auto& condition : json["when"].GetObject()) {
        hook.conditions.push_back(createCondition(condition.name.GetString(),
                                                  condition.value));
    }

    for(const auto& stage : json["stages"].GetArray()) {
        hook.stages.push_back(stage.GetString());
    }

    utility::logMessage(boost::format{"Successfully created OCI hook object"},
                        common::LogLevel::INFO);

    return hook;
}

std::unique_ptr<OCIHook::Condition> OCIHooksFactory::createCondition(const std::string& name,
                                                                     const rapidjson::Value& value) const {
    if(name == "always") {
        return std::unique_ptr<OCIHook::Condition>{ new OCIHook::ConditionAlways{ value.GetBool() } };
    }
    else if(name == "annotations") {
        auto annotations = std::vector<std::tuple<std::string, std::string>>{};
        for(const auto& annotation : value.GetObject()) {
            annotations.emplace_back(annotation.name.GetString(), annotation.value.GetString());
        }
        return std::unique_ptr<OCIHook::Condition>{ new OCIHook::ConditionAnnotations{ annotations } };
    }
    else if(name == "commands") {
        auto commands = std::vector<std::string>{};
        for(const auto& command : value.GetArray()) {
            commands.push_back(command.GetString());
        }
        return std::unique_ptr<OCIHook::Condition>{ new OCIHook::ConditionCommands{ commands } };
    }
    else if(name == "hasBindMounts") {
        return std::unique_ptr<OCIHook::Condition>{ new OCIHook::ConditionHasBindMounts{ value.GetBool() } };
    }
    else {
        auto message = boost::format("Unexpected condition \"%s\" in OCI hook's JSON") % name;
        SARUS_THROW_ERROR(message.str());
    }
}

}} // namespaces
