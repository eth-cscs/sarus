/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_utility_hook_hpp
#define libsarus_utility_hook_hpp

#include <tuple>
#include <string>
#include <unordered_map>
#include <sys/types.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <rapidjson/document.h>

#include "libsarus/Logger.hpp"
#include "libsarus/UserIdentity.hpp"

/**
 * Utility functions for hooks 
 */

namespace libsarus {
namespace hook {

struct ContainerState {
  ContainerState() = default;
  explicit ContainerState(std::istream&);
  std::string id() const;
  std::string status() const;
  pid_t pid() const;
  boost::filesystem::path bundle() const;

  private:
   rapidjson::Document state;
};

void applyLoggingConfigIfAvailable(const rapidjson::Document&);
ContainerState parseStateOfContainerFromStdin();
std::unordered_map<std::string, std::string> parseEnvironmentVariablesFromOCIBundle(const boost::filesystem::path&);
boost::optional<std::string> getEnvironmentVariableValueFromOCIBundle(const std::string& key, const boost::filesystem::path&);
void enterMountNamespaceOfProcess(pid_t);
void enterPidNamespaceOfProcess(pid_t pid);
void validatedBindMount(const boost::filesystem::path& from, const boost::filesystem::path& to,
        const libsarus::UserIdentity& userIdentity, const boost::filesystem::path& bundleDir, const boost::filesystem::path& rootfsDir);
std::tuple<boost::filesystem::path, boost::filesystem::path> findSubsystemMountPaths(const std::string& subsystemName,
        const boost::filesystem::path& procPrefixDir, const pid_t pid);
boost::filesystem::path findCgroupPathInHierarchy(const std::string& subsystemName, const boost::filesystem::path& procPrefixDir,
        const boost::filesystem::path& subsystemMountRoot, const pid_t pid);
boost::filesystem::path findCgroupPath(const std::string& subsystemName, const boost::filesystem::path& procPrefixDir, const pid_t pid);
void whitelistDeviceInCgroup(const boost::filesystem::path& cgroupPath, const boost::filesystem::path& deviceFile);
void switchToUnprivilegedProcess(const uid_t targetUid, const gid_t targetGid);
std::tuple<unsigned int, unsigned int> parseLibcVersionFromLddOutput(const std::string& lddOutput);
void logMessage(const boost::format& message, libsarus::LogLevel level,
        std::ostream& out=std::cout, std::ostream& err=std::cerr);
void logMessage(const std::string& message, libsarus::LogLevel level,
        std::ostream& out=std::cout, std::ostream& err=std::cerr);

}}

#endif
