/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
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
        SARUS_THROW_ERROR(message.str());
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
        SARUS_THROW_ERROR(message.str());
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
        SARUS_THROW_ERROR(message.str());
    }

    auto imageID = common::ImageID{ server, repositoryNamespace, image, tag };

    printLog(boost::format("successfully parsed image ID %s") % imageID, common::LogLevel::DEBUG);

    return imageID;
}

static bool hasDashPrefix(const char* s) {
    bool result = strlen(s) > 1 && s[0]=='-' && s[1]!='-';
    return result;
}

static bool hasDashDashPrefix(const char* s) {
    bool result = strlen(s) > 2 && s[0]=='-' && s[1]=='-' && s[2]!='-';
    return result;
}

static bool isOption(const char* s) {
    return hasDashPrefix(s) || hasDashDashPrefix(s);
}

static bool optionTakesValue(const boost::program_options::option_description* option) {
    bool result = option->semantic()->max_tokens() > 0;
    return result;
}

static common::CLIArguments::const_iterator processPossibleValueInNextToken(common::CLIArguments::const_iterator arg,
        common::CLIArguments::const_iterator argsEnd, common::CLIArguments& argsGroup) {
    // always include the current token (the option)
    argsGroup.push_back(*arg);

    // if next arg token exists and does not start with dash it's the value: include it and skip over
    auto nextArg = arg+1;
    if (nextArg != argsEnd) {
        if(!hasDashPrefix(*nextArg)){
            argsGroup.push_back(*nextArg);
            ++arg;
        }
    }

    return arg;
}

static common::CLIArguments::const_iterator processDashDashOption(common::CLIArguments::const_iterator arg,
        common::CLIArguments::const_iterator argsEnd, common::CLIArguments& argsGroup,
        const boost::program_options::options_description& optionsDescription) {
    auto argString = std::string{*arg};

    // if token contains '=' then it's using adjacent style and already provides a value
    if(argString.find('=') != std::string::npos) {
        argsGroup.push_back(argString);
    }
    else {
        // extract name by removing "--" prefix
        auto argName = argString.substr(2);

        // find if this is an option for the current command
        auto argOption = optionsDescription.find_nothrow(argName, false);

        // not an option: include and continue to next token (Boost will detect error about wrong option)
        if(!argOption) {
            argsGroup.push_back(*arg);
            return arg;
        }

        // check if option might take a value
        if(optionTakesValue(argOption)) {
            arg = processPossibleValueInNextToken(arg, argsEnd, argsGroup);
        }
        else {
            argsGroup.push_back(*arg);
        }
    }

    return arg;
}

static common::CLIArguments::const_iterator processDashOption(common::CLIArguments::const_iterator arg,
        common::CLIArguments::const_iterator argsEnd, common::CLIArguments& argsGroup,
        const boost::program_options::options_description& optionsDescription) {
    auto argString = std::string{*arg};

    // remove '-' prefix
    auto argSubstring = argString.substr(1);

    for(auto it = argSubstring.cbegin(); it != argSubstring.cend(); ++it) {
        // find if this is an option for the current command
        auto findArg = std::string{"-"} + *it;
        auto argOption = optionsDescription.find_nothrow(findArg, false);

        // not an option: include and continue to next token (Boost will detect error about wrong option)
        if(!argOption) {
            argsGroup.push_back(*arg);
            break;
        }

        // check if option might take a value
        if(optionTakesValue(argOption)) {
            // is token finished?
            if(it+1 == argSubstring.end()) {
                arg = processPossibleValueInNextToken(arg, argsEnd, argsGroup);
            }
            else {
                argsGroup.push_back(*arg);
                break;
            }
        }
        else {
            // current option takes no value. If token continues it could contain "sticky" short options
            // is token finished?
            if(it+1 == argSubstring.end()) {
                argsGroup.push_back(*arg);
            }
            else {
                // analyze next character
                continue;
            }
        }
    }

    return arg;
}

/**
 * Group option arguments and positional arguments into two individual CLIArguments objects.
 *
 * The first group contains the program/command name, its options and their values, if present;
 * it is meant to be further processed by boost::program_option functions.
 * The second group contains all the arguments from the first detected positional argument
 * (not an option or a value) onwards.
 * The second group may contain options for subcommands or container applications; such options
 * are not parsed by this function.
 * The second group can be passed around to access subcommand arguments and parse them appropriately.
 *
 * If there are no positional arguments, the second CLIArguments object is empty.
 *
 * The style used for identifying options and the terminology (e.g. "short", "long", "sticky") takes
 * as reference the UNIX style of boost::program_options:
 * https://www.boost.org/doc/libs/1_65_0/doc/html/boost/program_options/command_line_style/style_t.html
 * The same style is also used in the Command classes for parsing the command line with Boost.
 *
 * E.g. the CLI arguments "sarus --verbose run --mpi image command" are grouped into
 * two CLIArguments objects ("sarus --verbose", "run --mpi image command").
 */
std::tuple<common::CLIArguments, common::CLIArguments> groupOptionsAndPositionalArguments(
        const common::CLIArguments& args,
        const boost::program_options::options_description& optionsDescription) {

    common::CLIArguments nameAndOptionArgs, positionalArgs;

    if(args.argc() == 0) {
        return std::tuple<common::CLIArguments, common::CLIArguments>{nameAndOptionArgs, positionalArgs};
    }

    // Initialize the first arguments group with the first input argument (the name of the program or command)
    assert(!isOption(args.argv()[0]));
    nameAndOptionArgs.push_back(args.argv()[0]);

    // Start analysis from second argument
    for(auto arg = args.begin()+1; arg != args.end(); ++arg) {
        if(!isOption(*arg)) {
            positionalArgs = common::CLIArguments{arg, args.end()};
            break;
        }

        if(hasDashDashPrefix(*arg)) {
            arg = processDashDashOption(arg, args.end(), nameAndOptionArgs, optionsDescription);
        }
        else if(hasDashPrefix(*arg)) {
            arg = processDashOption(arg, args.end(), nameAndOptionArgs, optionsDescription);
        }
    }

    return std::tuple<common::CLIArguments, common::CLIArguments>{nameAndOptionArgs, positionalArgs};
}

void validateNumberOfPositionalArguments(const common::CLIArguments& positionalArgs, const int min, const int max,
        const std::string& command) {
    auto numberOfArguments = positionalArgs.argc();
    if(numberOfArguments < min || numberOfArguments > max) {
        auto quantity = numberOfArguments < min ? std::string("few") : std::string("many");
        auto message = boost::format("Too %s arguments for command '%s'\n"
                                     "See 'sarus help %s'") % quantity % command % command;
        printLog(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
    }
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
