/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "cli/Utility.hpp"

#include <chrono>

#include <boost/program_options.hpp>
#include <boost/algorithm/string/join.hpp>

#include "common/Config.hpp"
#include "common/Utility.hpp"
#include "cli/MountParser.hpp"
#include "cli/regex.hpp"


namespace sarus {
namespace cli {
namespace utility {

/**
 * Parse server, namespace and individual image name from a string matching cli::regex::name
 */
static std::tuple<std::string, std::string, std::string> parseNameMatch(const std::string& in) {
    auto server = common::ImageReference::DEFAULT_SERVER;
    auto repositoryNamespace = common::ImageReference::DEFAULT_REPOSITORY_NAMESPACE;
    auto image = std::string{};
    auto first_separator = in.find_first_of("/");
    auto last_separator = in.find_last_of("/");

    // No separators found: input is short image name
    if (last_separator == std::string::npos){
        image = in;
    }
    // Only one separator: input is "namespace/image"
    else if (first_separator == last_separator){
        repositoryNamespace = in.substr(0, first_separator);
        image = in.substr(last_separator+1);
    }
    // Two or more separators
    else {
        server = in.substr(0, first_separator);
        repositoryNamespace = in.substr(first_separator + 1, last_separator - first_separator - 1);
        image = in.substr(last_separator+1);
    }

    return std::tuple<std::string, std::string, std::string>{server, repositoryNamespace, image};
}

/**
 * Validates the image reference read through CLI.
 * Returns true if the image reference is valid, otherwise false.
 * 
 * An invalid image reference contains the pattern "..", which could
 * be exploited by a malicious user to access data outside the
 * Sarus's repository folder. E.g. if the image reference is
 * "../../image:tag", then the resulting unique key would be
 * "../../image/tag".
 */
bool isValidCLIInputImageReference(const std::string& imageReference) {
    boost::cmatch matches;
    boost::regex disallowedPattern(".*\\.\\..*");

    if (boost::regex_match(imageReference.c_str(), matches, disallowedPattern)) {
        return false;
    }
    return true;
}

/**
 * Parse the CLI arguments corresponding to an image reference
 */
common::ImageReference parseImageReference(const common::CLIArguments& imageArgs) {
    if(imageArgs.argc() != 1) {
        auto message = boost::format(
            "Invalid image ID %s\n"
            "The image ID is expected to be a single token without options") % imageArgs;
        SARUS_THROW_ERROR(message.str());
    }

    return parseImageReference(imageArgs.argv()[0]);
}

/**
 * Parse the input reference of a container image
 */
common::ImageReference parseImageReference(const std::string &input) {
    printLog( boost::format("Parsing image ID from string: %s") % input, common::LogLevel::DEBUG);

    auto server              = std::string{};
    auto repositoryNamespace = std::string{};
    auto image               = std::string{};
    auto tag                 = std::string{};
    auto digest              = std::string{};

    if(!isValidCLIInputImageReference(input)) {
        auto message = boost::format("Invalid image ID '%s'\n"
                                     "Image IDs are not allowed to contain the sequence '..'") % input;
        SARUS_THROW_ERROR(message.str());
    }

    boost::smatch matches;
    if (boost::regex_match(input, matches, regex::reference)) {
        auto nameMatch   = matches[1];
        auto tagMatch    = matches[2];
        auto digestMatch = matches[3];

        std::tie(server, repositoryNamespace, image) = parseNameMatch(nameMatch);

        if (tagMatch.matched) {
            tag = tagMatch.str();
        }
        else {
            // If there is not a digest, use default tag
            // If there is a digest, the tag does not matter
            if (!digestMatch.matched) {
                tag = common::ImageReference::DEFAULT_TAG;
            }
        }

        if (digestMatch.matched) {
            digest = digestMatch.str();
        }
    }
    else {
        auto message = boost::format("Invalid image ID '%s'") % input;
        SARUS_THROW_ERROR(message.str());
    }

    auto imageReference = common::ImageReference{server, repositoryNamespace, image, tag, digest};
    printLog(boost::format("Successfully parsed image ID %s") % imageReference, common::LogLevel::DEBUG);

    return imageReference;
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
