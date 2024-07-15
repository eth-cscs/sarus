
/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "string.hpp"

#include <iostream>
#include <random>
#include <vector>

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#include "libsarus/Error.hpp"
#include "libsarus/utility/logging.hpp"

/**
 * Utility functions for string manipulation 
 */

namespace libsarus {
namespace string {

std::string removeWhitespaces(const std::string& s) {
    auto result = s;
    auto newEnd = std::remove_if(result.begin(), result.end(), iswspace);
    result.erase(newEnd, result.end());
    return result;
}

std::string replace(std::string &buf, const std::string& from, const std::string& to) {
    std::string::size_type pos = buf.find(from);
    while(pos != std::string::npos){
        buf.replace(pos, from.size(), to);
        pos = buf.find(from, pos + to.size());
    }
    return buf;
}

std::string eraseFirstAndLastDoubleQuote(const std::string& s) {
    if(s.size() < 2 || *s.cbegin() != '"' || *s.crbegin() != '"') {
        auto message = boost::format(   "Failed to remove first and last double quotes"
                                        " in string \"%s\". The string doesn't"
                                        " contain such double quotes.") % s;
        SARUS_THROW_ERROR(message.str());
    }
    return std::string( s.cbegin() + 1,
                        s.cbegin() + s.size() - 1);
}

std::pair<std::string, std::string> parseKeyValuePair(const std::string& pairString, const char separator) {
    auto keyEnd = std::find(pairString.cbegin(), pairString.cend(), separator);
    auto key = std::string(pairString.cbegin(), keyEnd);
    auto value = keyEnd != pairString.cend() ? std::string(keyEnd+1, pairString.cend()) : std::string{};
    if(key.empty()) {
        auto message = boost::format("Failed to parse key-value pair '%s': key is empty") % pairString;
        SARUS_THROW_ERROR(message.str())
    }
    return std::pair<std::string, std::string>{key, value};
}

std::string generateRandom(size_t size) {
    auto dist = std::uniform_int_distribution<std::mt19937::result_type>(0, 'z'-'a');
    std::mt19937 generator;
    generator.seed(std::random_device()());

    auto string = std::string(size, '.');

    for(size_t i=0; i<string.size(); ++i) {
        auto randomCharacter = 'a' + dist(generator);
        string[i] = randomCharacter;
    }

    return string;
}

/**
 * Converts a string representing a list of key-value pairs to a map.
 *
 * If no separators are passed as arguments, the pairs are assumed to be separated by commas,
 * while keys and values are assumed to be separated by an = sign.
 * If a value is not specified (i.e. a character sequence between two pair separators does
 * not feature a key-value separator), the map entry is created with the value as an
 * empty string.
 */
std::unordered_map<std::string, std::string> parseMap(const std::string& input,
                                                      const char pairSeparators,
                                                      const char keyValueSeparators) {
    if(input.empty()) {
        return std::unordered_map<std::string, std::string>{};
    }

    auto map = std::unordered_map<std::string, std::string>{};

    auto pairs = std::vector<std::string>{};
    boost::split(pairs, input, boost::is_any_of(std::string{pairSeparators}));

    for(const auto& pair : pairs) {
        std::string key, value;
        try {
            std::tie(key, value) = string::parseKeyValuePair(pair, keyValueSeparators);
        }
        catch(std::exception& e) {
            auto message = boost::format("Error parsing '%s'. %s") % input % e.what();
            logMessage(message, libsarus::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
        }

        // do not allow repeated separators in the value
        auto valueEnd = std::find(value.cbegin(), value.cend(), keyValueSeparators);
        if(valueEnd != value.cend()) {
            auto message = boost::format("Error parsing '%s'. Invalid key-value pair '%s': repeated use of separator is not allowed.")
                % input % pair;
            logMessage(message, libsarus::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO)
        }

        // check for duplicated key
        if(map.find(key) != map.cend()) {
            auto message = boost::format("Error parsing '%s'. Found duplicated key '%s': expected a list of unique key-value pairs.")
                % input % key;
            logMessage(message, libsarus::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO)
        }

        map[key] = value;
    }

    return map;
}

}}
