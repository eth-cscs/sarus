/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _common_Utility_hpp
#define _common_Utility_hpp

#include <cstdlib>
#include <string>
#include <tuple>
#include <unordered_map>
#include <sys/types.h>

#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <cpprest/json.h>
#include <rapidjson/document.h>
#include <rapidjson/schema.h>

#include "common/Config.hpp"
#include "common/Logger.hpp"

/**
 * Utility functions
 */

namespace sarus {
namespace common {

std::unordered_map<std::string, std::string> parseEnvironmentVariables(char** env);
std::tuple<std::string, std::string> parseEnvironmentVariable(const std::string& variable);
std::string getEnvironmentVariable(const std::string& key);
void setEnvironmentVariable(const std::string& variable);
std::string removeWhitespaces(const std::string&);
std::string replaceString(std::string &buf, const std::string& from, const std::string& to);
std::string eraseFirstAndLastDoubleQuote(const std::string& buf);
std::string executeCommand(const std::string& command);
void forkExecWait(const common::CLIArguments& args, const boost::optional<boost::filesystem::path>& chrootJail = {});
void SetStdinEcho(bool);
std::string getHostname();
std::string getUsername(uid_t uid);
size_t getFileSize(const boost::filesystem::path& filename);
std::tuple<uid_t, gid_t> getOwner(const boost::filesystem::path&);
void setOwner(const boost::filesystem::path&, uid_t, gid_t);
bool isCentralizedRepositoryEnabled(const common::Config& config);
boost::filesystem::path getCentralizedRepositoryDirectory(const common::Config& config);
boost::filesystem::path getLocalRepositoryDirectory(const common::Config& config);
boost::filesystem::path getLocalRepositoryDirectory(const boost::filesystem::path& baseDir, uid_t uid);
boost::filesystem::path makeUniquePathWithRandomSuffix(const boost::filesystem::path&);
std::string generateRandomString(size_t size);
void createFoldersIfNecessary(const boost::filesystem::path&, uid_t uid=-1, gid_t gid=-1);
void createFileIfNecessary(const boost::filesystem::path&, uid_t uid=-1, gid_t gid=-1);
void copyFile(const boost::filesystem::path& src, const boost::filesystem::path& dst, uid_t uid=-1, gid_t gid=-1);
void copyFolder(const boost::filesystem::path& src, const boost::filesystem::path& dst, uid_t uid=-1, gid_t gid=-1);
void changeDirectory(const boost::filesystem::path& path);
boost::filesystem::path realpathWithinRootfs(const boost::filesystem::path& rootfs, const boost::filesystem::path& path);
std::unordered_map<std::string, std::string> convertListOfKeyValuePairsToMap(const std::string& kvList,
        const char pairSeparator = ',', const char kvSeparator = '=');
std::string makeColonSeparatedListOfPaths(const std::vector<boost::filesystem::path>& paths);

/**
 * Converts a string representing a list of entries to a vector of strings.
 *
 * If no separator is passed as argument, the entries are assumed to be separated by semicolons.
 */
template<class T>
std::vector<T> convertStringListToVector(const std::string& input_string, const char separator = ';') {
    auto vec = std::vector<T>{};
    
    auto is_not_separator = [separator](char c) {
        return c != separator;
    };
    
    auto entryBegin = std::find_if(input_string.cbegin(), input_string.cend(), is_not_separator);
    while(entryBegin != input_string.cend()) {
        auto entryEnd = std::find(entryBegin, input_string.cend(), separator);
        auto entry = std::string(entryBegin, entryEnd);
        vec.emplace_back(entry);

        entryBegin = entryEnd;
        entryBegin = std::find_if(entryBegin, input_string.cend(), is_not_separator);
    }

    return vec;
}

std::vector<boost::filesystem::path> getLibrariesFromDynamicLinker(
    const boost::filesystem::path& ldconfigPath,
    const boost::filesystem::path& rootDir);
std::string getLibrarySoname(const boost::filesystem::path& path, const boost::filesystem::path& readelfPath);
bool isLibc(const boost::filesystem::path&);
std::tuple<unsigned int, unsigned int> parseLibcVersion(const boost::filesystem::path&);
bool is64bitLibrary(const boost::filesystem::path& path, const boost::filesystem::path& readelfPath);

std::string readFile(const boost::filesystem::path& path);
rapidjson::Document readJSON(const boost::filesystem::path& filename);
rapidjson::Document readAndValidateJSON(const boost::filesystem::path& configFilename,
                                        const rapidjson::SchemaDocument& schema);
void writeJSON(const rapidjson::Value& json, const boost::filesystem::path& filename);
std::string serializeJSON(const rapidjson::Value& json);
rapidjson::Document convertCppRestJsonToRapidJson(web::json::value&);
void logMessage(const std::string&, LogLevel);
void logMessage(const boost::format&, LogLevel);

} // namespace
} // namespace

#endif
