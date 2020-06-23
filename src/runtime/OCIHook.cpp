/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "OCIHook.hpp"

#include <boost/regex.hpp>

#include "runtime/Utility.hpp"
#include "runtime/ConfigsMerger.hpp"

namespace sarus {
namespace runtime {

OCIHook::ConditionAlways::ConditionAlways(bool value)
    : value{ value }
{
    utility::logMessage(boost::format{"Created OCI Hook's \"always\" condition (%s)"}
                        % boost::io::group(std::boolalpha, value),
                        common::LogLevel::DEBUG);
}

bool OCIHook::ConditionAlways::evaluate(std::shared_ptr<const common::Config>) const {
    utility::logMessage(boost::format{"OCI Hook's \"always\" condition evaluates \"%s\""}
                        % boost::io::group(std::boolalpha, value),
                        common::LogLevel::DEBUG);
    return value;
}

OCIHook::ConditionAnnotations::ConditionAnnotations(const std::vector<std::tuple<std::string, std::string>>& annotations)
    : annotations{ annotations }
{
    utility::logMessage(boost::format{"Created OCI Hook's \"annotations\" condition"},
                        common::LogLevel::DEBUG);
}

bool OCIHook::ConditionAnnotations::evaluate(std::shared_ptr<const common::Config> config) const {
    utility::logMessage(boost::format{"Evaluating OCI Hook's \"annotations\" condition"},
                        common::LogLevel::DEBUG);

    auto configsMerger = ConfigsMerger{config, common::ImageMetadata{ config->getMetadataFileOfImage() }};
    auto bundleAnnotations = configsMerger.getBundleAnnotations();

    for(const auto& annotation : annotations) {
        auto keyRegex = boost::regex{ std::get<0>(annotation) };
        auto valueRegex = boost::regex{ std::get<1>(annotation) };
        auto matches = boost::smatch{};

        bool matchFound = false;

        for(const auto& bundleAnnotation : bundleAnnotations) {
            utility::logMessage(boost::format{"Processing bundle's annotation {%s: %s}"}
                                % std::get<0>(bundleAnnotation)
                                % std::get<1>(bundleAnnotation),
                                common::LogLevel::DEBUG);

            if(boost::regex_match(std::get<0>(bundleAnnotation), matches, keyRegex) &&
               boost::regex_match(std::get<1>(bundleAnnotation), matches, valueRegex)) {
                matchFound = true;
                break;
            }
        }

        utility::logMessage(boost::format{"Annotation {\"%s\": \"%s\"} evaluates \"%s\""}
                            % std::get<0>(annotation) % std::get<1>(annotation)
                            % boost::io::group(std::boolalpha, matchFound),
                            common::LogLevel::DEBUG);

        if(!matchFound) {
            utility::logMessage(boost::format{"OCI Hook's \"annotations\" condition evaluates \"false\""},
                        common::LogLevel::DEBUG);
            return false;
        }
    }

    utility::logMessage(boost::format{"OCI Hook's \"annotations\" condition evaluates \"true\""},
                        common::LogLevel::DEBUG);

    return true;
}

OCIHook::ConditionCommands::ConditionCommands(const std::vector<std::string>& commands)
    : commands{ commands }
{
    utility::logMessage(boost::format{"Created OCI Hook's \"commands\" condition"},
                        common::LogLevel::DEBUG);
}

bool OCIHook::ConditionCommands::evaluate(std::shared_ptr<const common::Config> config) const {
    utility::logMessage(boost::format{"Evaluating OCI Hook's \"commands\" condition"},
                        common::LogLevel::DEBUG);

    auto configsMerger = ConfigsMerger{config, common::ImageMetadata{ config->getMetadataFileOfImage() }};
    auto arg0 = std::string(configsMerger.getCommandToExecuteInContainer().argv()[0]);

    for(const auto& command : commands) {
        auto regex = boost::regex{ command };
        auto matches = boost::smatch{};
        if(boost::regex_match(arg0, matches, regex)) {
            utility::logMessage(boost::format{"Command regex \"%s\" matches (arg0=\"%s\")"} % command % arg0,
                                common::LogLevel::DEBUG);
            utility::logMessage(boost::format{"OCI Hook's \"commands\" condition evaluates \"true\""},
                                common::LogLevel::DEBUG);
            return true;
        }
        else {
            utility::logMessage(boost::format{"Command regex \"%s\" doesn't match (arg0=\"%s\")"} % command % arg0,
                                common::LogLevel::DEBUG);
        }
    }

    utility::logMessage(boost::format{"OCI Hook's \"commands\" condition evaluates \"false\""},
                        common::LogLevel::DEBUG);

    return false;
}

OCIHook::ConditionHasBindMounts::ConditionHasBindMounts(bool value)
    : value{ value }
{
    utility::logMessage(boost::format{"Created OCI Hook's \"hasBindMounts\" condition"},
                        common::LogLevel::DEBUG);
}

bool OCIHook::ConditionHasBindMounts::evaluate(std::shared_ptr<const common::Config> config) const {
    bool result = value != config->commandRun.mounts.empty();

    utility::logMessage(boost::format{"OCI Hook's \"hasBindMounts\" condition evaluates \"%s\""}
                        % boost::io::group(std::boolalpha, result),
                        common::LogLevel::DEBUG);

    return result;
}

bool OCIHook::isActive(std::shared_ptr<const common::Config> config) const {
    utility::logMessage(boost::format{"Evaluating \"when\" conditions of OCI Hook %s"} % jsonFile,
                        common::LogLevel::INFO);

    for(const auto& condition : conditions) {
        if(!condition->evaluate(config)) {
            utility::logMessage("OCI Hook is inactive", common::LogLevel::INFO);
            return false;
        }
    }

    utility::logMessage("OCI Hook is active", common::LogLevel::INFO);

    return true;
}

}} // namespaces
