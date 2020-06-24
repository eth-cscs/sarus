/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
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
#include <functional>
#include <unordered_map>
#include <sys/types.h>

#include <boost/optional.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <cpprest/json.h>
#include <rapidjson/document.h>
#include <rapidjson/schema.h>

#include "common/Config.hpp"
#include "common/Logger.hpp"
#include "common/UserIdentity.hpp"
#include "common/AbiCompatibility.hpp"

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
void switchToUnprivilegedUser(const common::UserIdentity&);
void switchToPrivilegedUser(const common::UserIdentity&);
std::string executeCommand(const std::string& command);
int forkExecWait(const common::CLIArguments& args,
                 const boost::optional<std::function<void()>>& preExecActions = {});
void SetStdinEcho(bool);
std::string getHostname();
size_t getFileSize(const boost::filesystem::path& filename);
std::tuple<uid_t, gid_t> getOwner(const boost::filesystem::path&);
void setOwner(const boost::filesystem::path&, uid_t, gid_t);
bool isCentralizedRepositoryEnabled(const common::Config& config);
boost::filesystem::path getCentralizedRepositoryDirectory(const common::Config& config);
boost::filesystem::path getLocalRepositoryDirectory(const common::Config& config);
boost::filesystem::path makeUniquePathWithRandomSuffix(const boost::filesystem::path&);
std::string generateRandomString(size_t size);
void createFoldersIfNecessary(const boost::filesystem::path&, uid_t uid=-1, gid_t gid=-1);
void createFileIfNecessary(const boost::filesystem::path&, uid_t uid=-1, gid_t gid=-1);
void copyFile(const boost::filesystem::path& src, const boost::filesystem::path& dst, uid_t uid=-1, gid_t gid=-1);
void copyFolder(const boost::filesystem::path& src, const boost::filesystem::path& dst, uid_t uid=-1, gid_t gid=-1);
void changeDirectory(const boost::filesystem::path& path);
int countFilesInDirectory(const boost::filesystem::path& path);
boost::filesystem::path realpathWithinRootfs(const boost::filesystem::path& rootfs, const boost::filesystem::path& path);
std::unordered_map<std::string, std::string> parseMap(const std::string& input,
                                                      const std::string& pairSeparators = ",",
                                                      const std::string& keyValueSeparators = "=");
std::string makeColonSeparatedListOfPaths(const std::vector<boost::filesystem::path>& paths);
boost::filesystem::path getSharedLibLinkerName(const boost::filesystem::path& path);
std::vector<boost::filesystem::path> getSharedLibsFromDynamicLinker(
    const boost::filesystem::path& ldconfigPath,
    const boost::filesystem::path& rootDir);
bool isSharedLib(const boost::filesystem::path& file);
std::vector<std::string> parseSharedLibAbi(const boost::filesystem::path& lib);
std::vector<std::string> resolveSharedLibAbi(const boost::filesystem::path& lib,
                                           const boost::filesystem::path& rootDir = "/");
std::string getSharedLibSoname(const boost::filesystem::path& path, const boost::filesystem::path& readelfPath);
bool isLibc(const boost::filesystem::path&);
std::tuple<unsigned int, unsigned int> parseLibcVersion(const boost::filesystem::path&);
bool is64bitSharedLib(const boost::filesystem::path& path, const boost::filesystem::path& readelfPath);
std::string parseCpusAllowedList(const std::string& procStatus);

std::string readFile(const boost::filesystem::path& path);
rapidjson::SchemaDocument readJSONSchema(const boost::filesystem::path& schemaFile);
rapidjson::Document readJSON(const boost::filesystem::path& filename);
rapidjson::Document readAndValidateJSON(const boost::filesystem::path& jsonFile,
                                        const boost::filesystem::path& schemaFile);
void writeJSON(const rapidjson::Value& json, const boost::filesystem::path& filename);
std::string serializeJSON(const rapidjson::Value& json);
rapidjson::Document convertCppRestJsonToRapidJson(web::json::value&);
void logMessage(const std::string&, LogLevel, std::ostream& out = std::cout, std::ostream& err = std::cerr);
void logMessage(const boost::format&, LogLevel, std::ostream& out = std::cout, std::ostream& err = std::cerr);

} // namespace
} // namespace

#endif
