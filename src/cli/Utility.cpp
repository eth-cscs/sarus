/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "cli/Utility.hpp"

#include <chrono>

#include <boost/regex.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/join.hpp>

#include "common/Config.hpp"
#include "common/Utility.hpp"
#include "cli/MountParser.hpp"


namespace sarus {
namespace cli {
namespace utility {

/**
 * Get image's name from "image<:tag>"
 */
static std::string getImageName(const std::string& in) {
    if ( in.find(":") == std::string::npos) {
        return in;
    }
    return in.substr( 0, in.find_last_of(":") );
}

/**
 * Get image's tag from "image<:tag>"
 */
static std::string getImageTag(const std::string& in) {
    if ( in.find(":") == std::string::npos) {
        return "latest";
    }
    return in.substr( in.find_last_of(":") + 1, in.size() - in.find_last_of(":") - 1);
}

/**
 * Validates the image ID read through CLI.
 * Returns true if the image ID is valid, otherwise false.
 * 
 * An invalid image ID contains the pattern "..", which could
 * be exploited by a malicious user to access data outside the
 * Sarus's repository folder. E.g. if the image ID is
 * "../../image:tag", then the resulting unique key would be
 * "../../image/tag".
 */
bool isValidCLIInputImageID(const std::string& imageID) {
    boost::cmatch matches;
    boost::regex disallowedPattern(".*\\.\\..*");

    if (boost::regex_match(imageID.c_str(), matches, disallowedPattern)) {
        return false;
    }
    return true;
}

/**
 * Parse the input name of container image
 */ 
common::ImageID parseImageID(const common::CLIArguments& imageArgs) {
    if(imageArgs.argc() != 1) {
        auto message = boost::format(
            "Invalid image ID %s\n"
            "The image ID is expected to be a single token without options") % imageArgs;
        printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }

    return parseImageID(imageArgs.argv()[0]);
}

/**
 * Parse the input ID of container image
 */ 
common::ImageID parseImageID(const std::string &input) {
    printLog( boost::format("parsing image ID"), common::LogLevel::DEBUG);

    std::string server;
    std::string repositoryNamespace;
    std::string image;
    std::string tag;

    if(!isValidCLIInputImageID(input)) {
        auto message = boost::format("Invalid image ID '%s'\n"
                                    "Image IDs are not allowed to contain the sequence '..'") % input;
        printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }

    boost::regex withServer("(.*?)/(.*?)/(.*?)");
    boost::regex withNamespace("(.*?)/(.*?)");
    boost::cmatch matches;

    // matches pattern <hostname>/<repositoryNamespace>/image<:tag>
    if(boost::regex_match( input.c_str(), matches, withServer)) {
        server = matches[1].str();
        repositoryNamespace = matches[2].str();
        image  = getImageName(matches[3].str());
        tag    = getImageTag(matches[3].str());
    }
    // matches pattern <repositoryNamespace>/image<:tag>
    else if(boost::regex_match( input.c_str(), matches, withNamespace)) {
        server = common::ImageID::DEFAULT_SERVER;
        repositoryNamespace = matches[1];
        image  = getImageName(matches[2].str());
        tag    = getImageTag(matches[2].str());
    }
    // matches pattern image<:tag>
    else {
        server = common::ImageID::DEFAULT_SERVER;
        repositoryNamespace = common::ImageID::DEFAULT_REPOSITORY_NAMESPACE;
        image  = getImageName(input);
        tag    = getImageTag(input);
    }

    // if empty field exists, throw exception
    if(server == "" || repositoryNamespace == "" || image == "" || tag == "") {
        auto message = boost::format("Invalid image ID '%s'") % input;
        printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }

    auto imageID = common::ImageID{ server, repositoryNamespace, image, tag };

    printLog(boost::format("successfully parsed image ID %s") % imageID, common::LogLevel::DEBUG);

    return imageID;
}

void printLog(const std::string& message, common::LogLevel LogLevel, std::ostream& outStream, std::ostream& errStream) {
    auto systemName = "CLI";
    common::Logger::getInstance().log(message, systemName, LogLevel, outStream, errStream);
}

void printLog(const boost::format& message, common::LogLevel LogLevel, std::ostream& outStream, std::ostream& errStream) {
    printLog(message.str(), LogLevel, outStream, errStream);
}

} // namespace
} // namespace
} // namespace
