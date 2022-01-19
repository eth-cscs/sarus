/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "MountParser.hpp"

#include <limits>
#include <sstream>

#include <boost/filesystem.hpp>

#include "common/Error.hpp"
#include "cli/Utility.hpp"
#include "runtime/Mount.hpp"


namespace sarus {
namespace cli {

MountParser::MountParser(bool isUserMount, std::shared_ptr<const common::Config> conf)
    : conf{std::move(conf)}
{
    if (isUserMount && this->conf->json.HasMember("userMounts")) {
        // Retrieve settings from Config struct
        const rapidjson::Value& userMounts = this->conf->json["userMounts"];
        for (const auto& value : userMounts["notAllowedPrefixesOfPath"].GetArray()) {
            validationSettings.destinationDisallowedWithPrefix.push_back(value.GetString());
        }
        for (const auto& value : userMounts["notAllowedPaths"].GetArray()) {
            validationSettings.destinationDisallowedExact.push_back(value.GetString());
        }
    }
    else {
        validationSettings.destinationDisallowedWithPrefix = {};
        validationSettings.destinationDisallowedExact = {};
    }
}

/**
 * Parses a custom mount request into a Mount object.
 * The request comes in the form of a comma-separated list of key-value pairs.
 */
std::unique_ptr<runtime::Mount> MountParser::parseMountRequest(const std::unordered_map<std::string, std::string>& requestMap) {
    // The request has to specify the mount type
    if (requestMap.count("type") == 0) {
        auto message = boost::format("Invalid mount request '%s': 'type' must be specified")
            % convertRequestMapToString(requestMap);
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }

    // Parse sub-options for different mount types separately
    if (requestMap.at("type") == std::string{"bind"}) {
        return parseBindMountRequest(requestMap);
    }
    else {
        auto message = boost::format("Invalid mount request '%s': '%s' is not a valid mount type")
            % convertRequestMapToString(requestMap) % requestMap.at("type");
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }
}

std::unique_ptr<runtime::Mount> MountParser::parseBindMountRequest(const std::unordered_map<std::string, std::string>& requestMap) {
    auto source = getValidatedMountSource(requestMap);
    auto destination = getValidatedMountDestination(requestMap);
    auto flags = convertBindMountFlags(requestMap); // Parse the other sub-options into mount flags

    return std::unique_ptr<runtime::Mount>{new runtime::Mount{source, destination, flags, conf}};
}

/*
 * Generates mount flags bitmask from a map that is expected to contain key-value
 * pairs representing auxiliary options for a custom bind mount.
 */
unsigned long MountParser::convertBindMountFlags(const std::unordered_map<std::string, std::string>& requestMap) {
    // Default to "recursive private" mount
    unsigned long flags = MS_REC | MS_PRIVATE;

    // Create local copy of map and remove sub-options not expected as mount flags
    auto flagsMap = requestMap;
    flagsMap.erase("type");
    flagsMap.erase("source");
    flagsMap.erase("src");
    flagsMap.erase("destination");
    flagsMap.erase("dst");
    flagsMap.erase("target");

    for (auto const& opt : flagsMap) {
        if (opt.first == std::string{"readonly"}) {
            flags |= MS_RDONLY;
        }
        else {
            auto message = boost::format("Invalid mount request '%s': '%s' is not a valid bind mount option")
                % convertRequestMapToString(requestMap) % opt.first;
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }
    }

    return flags;
}

boost::filesystem::path MountParser::getValidatedMountSource(const std::unordered_map<std::string, std::string>& requestMap) {
    boost::filesystem::path source;

    // Check that one and only one of the key variants for the source is in use
    // and retrieve its value
    if(requestMap.count("source") && requestMap.count("src")) {
        auto message = boost::format("Invalid mount request '%s': multiple formats used to specify mount source. "
                                     "Use either 'source' or 'src'.")
            % convertRequestMapToString(requestMap);
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }
    else if(requestMap.count("source")) {
        source = requestMap.at("source");
    }
    else if (requestMap.count("src")) {
        source = requestMap.at("src");
    }
    else {
        auto message = boost::format("Invalid mount request '%s': no source specified. Use either 'source' or 'src'.")
            % convertRequestMapToString(requestMap);
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }

    if (source.empty()) {
        auto message = boost::format("Invalid mount request '%s': source is empty")
            % convertRequestMapToString(requestMap);
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }

    if (source.is_relative()) {
        auto message = boost::format("Invalid mount request '%s': source must be an absolute path")
            % convertRequestMapToString(requestMap);
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }

    for (const auto& disallowed_prefix : validationSettings.sourceDisallowedWithPrefix) {
        if (source.string().find(disallowed_prefix) == 0) {
            auto message = boost::format("Invalid mount request '%s': source cannot be a subdirectory of '%s'")
                % convertRequestMapToString(requestMap)
                % source.string()
                % disallowed_prefix;
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }
    }
    for (const auto& disallowed : validationSettings.sourceDisallowedExact) {
        if (source.string() == disallowed) {
            auto message = boost::format("Invalid mount request '%s': '%s' is not allowed as mount source")
                % convertRequestMapToString(requestMap)
                % disallowed;
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }
    }

    return source;
}

boost::filesystem::path MountParser::getValidatedMountDestination(const std::unordered_map<std::string, std::string>& requestMap) {
    boost::filesystem::path destination;

    // Check that one and only one of the key variants for the destination is in use
    // and retrieve its value
    if(requestMap.count("destination") + requestMap.count("dst") + requestMap.count("target") > 1) {
        auto message = boost::format("Invalid mount request '%s': multiple formats used to specify mount destination."
                                     " Use one of 'destination', 'dst' or 'target'.")
            % convertRequestMapToString(requestMap);
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }
    else if (requestMap.count("destination")) {
        destination = requestMap.at("destination");
    }
    else if (requestMap.count("dst")) {
        destination = requestMap.at("dst");
    }
    else if (requestMap.count("target")) {
        destination = requestMap.at("target");
    }
    else {
        auto message = boost::format("Invalid mount request '%s': no destination specified. Use either 'destination', 'dst' or 'target'.")
            % convertRequestMapToString(requestMap);
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }

    if (destination.empty()) {
        auto message = boost::format("Invalid mount request '%s': destination is empty")
            % convertRequestMapToString(requestMap);
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }

    if (destination.is_relative()) {
        auto message = boost::format("Invalid mount request '%s': destination must be an absolute path")
            % convertRequestMapToString(requestMap);
        utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }

    for (const auto& disallowed_prefix : validationSettings.destinationDisallowedWithPrefix) {
        if (destination.string().find(disallowed_prefix) == 0) {
            auto message = boost::format("Invalid mount request '%s': destination cannot be a subdirectory of '%s'")
                % convertRequestMapToString(requestMap)
                % disallowed_prefix;
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }
    }
    for (const auto& disallowed : validationSettings.destinationDisallowedExact) {
        if (destination.string() == disallowed) {
            auto message = boost::format("Invalid mount request '%s': '%s' is not allowed as mount destination")
                % convertRequestMapToString(requestMap)
                % disallowed;
            utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }
    }

    return destination;
}

// used to produce log messages
std::string MountParser::convertRequestMapToString(const std::unordered_map<std::string, std::string>& map) const {
    std::stringstream s{};
    bool isFirstValue = true;
    for(const auto& value : map) {
        if(!isFirstValue) {
            s << ",";
        }
        else {
            isFirstValue = false;
        }
        s << value.first;
        if(!value.second.empty()) {
            s << "=" << value.second;
        }
    }
    return s.str();
}

} // namespace
} // namespace
