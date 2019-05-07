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
std::vector<std::string> convertStringListToVector(const std::string& input_string,
        const char separator = ';');
std::string readFile(const boost::filesystem::path& path);
rapidjson::Document readJSON(const boost::filesystem::path& filename);
rapidjson::Document readAndValidateJSON(const boost::filesystem::path& configFilename,
                                        const rapidjson::SchemaDocument& schema);
void writeJSON(const rapidjson::Value& json, const boost::filesystem::path& filename);
std::string serializeJSON(const rapidjson::Value& json);
rapidjson::Document convertCppRestJsonToRapidJson(web::json::value&);
void logMessage(const std::string&, logType);
void logMessage(const boost::format&, logType);

} // namespace
} // namespace

#endif
