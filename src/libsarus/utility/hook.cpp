/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "hook.hpp"

#include <iostream>
#include <cstring>
#include <grp.h>
#include <istream>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

#include <rapidjson/istreamwrapper.h>

#include "libsarus/Error.hpp"
#include "libsarus/utility/environment.hpp"
#include "libsarus/utility/filesystem.hpp"
#include "libsarus/utility/json.hpp"

/**
 * Utility functions for hooks 
 */

namespace rj = rapidjson;

namespace libsarus {
namespace hook {

ContainerState::ContainerState(std::istream& is) {
    rj::IStreamWrapper sw(is);
    state.ParseStream(sw);
};

std::string ContainerState::id() const { return state["id"].GetString(); };

std::string ContainerState::status() const { return state["status"].GetString(); };

pid_t ContainerState::pid() const {
    if(state.HasMember("pid")) {
        return state["pid"].GetInt();
    }
    return -1;
}

boost::filesystem::path ContainerState::bundle() const {
    return boost::filesystem::path{state["bundle"].GetString()};
};


static void replaceFd(int oldfd, int newfd) {
    if(dup2(newfd, oldfd) == -1) {
        auto message = boost::format("Failed to replace fd with 'dup(%d, %d)': %s")
            % oldfd
            % newfd
            % std::strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}

void applyLoggingConfigIfAvailable(const rapidjson::Document& json) {
    if(!json.HasMember("annotations")) {
        return;
    }

    if(json["annotations"].HasMember("com.hooks.logging.level")) {
        auto str = json["annotations"]["com.hooks.logging.level"].GetString();
        auto level = static_cast<libsarus::LogLevel>(std::stoi(str));
        libsarus::Logger::getInstance().setLevel(level);
    }

    if(json["annotations"].HasMember("com.hooks.logging.stdoutfd")) {
        int fd = open(json["annotations"]["com.hooks.logging.stdoutfd"].GetString(), O_WRONLY);
        replaceFd(1, fd);
        close(fd);
    }

    if(json["annotations"].HasMember("com.hooks.logging.stderrfd")) {
        int fd = open(json["annotations"]["com.hooks.logging.stderrfd"].GetString(), O_WRONLY);
        replaceFd(2, fd);
        close(fd);
    }
}

ContainerState parseStateOfContainerFromStdin() {
    try {
        return ContainerState{std::cin};
    }
    catch(const std::exception& e) {
        SARUS_RETHROW_ERROR(e, "Failed to parse container's state JSON from stdin.");
    }
}

std::unordered_map<std::string, std::string> parseEnvironmentVariablesFromOCIBundle(const boost::filesystem::path& bundleDir) {
    auto env = std::unordered_map<std::string, std::string>{};
    auto json = json::read(bundleDir / "config.json");
    for(const auto& variable : json["process"]["env"].GetArray()) {
        std::string k, v;
        std::tie(k, v) = environment::parseVariable(variable.GetString());
        env[k] = v;
    }
    return env;
}

boost::optional<std::string> getEnvironmentVariableValueFromOCIBundle(const std::string& key, const boost::filesystem::path& bundleDir) {
    boost::optional<std::string> value;
    auto json = json::read(bundleDir / "config.json");
    for(const auto& variable : json["process"]["env"].GetArray()) {
        std::string k, v;
        std::tie(k, v) = environment::parseVariable(variable.GetString());
        if (k == key) {
            value = v;
            break;
        }
    }
    return value;
}

static void enterNamespace(const boost::filesystem::path& namespaceFile) {
    // get namespace's fd   
    auto fd = open(namespaceFile.c_str(), O_RDONLY);
    if (fd == -1) {
        auto message = boost::format("Failed to open namespace file %s: %s") % namespaceFile % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    // enter namespace
    if (setns(fd, 0) != 0) {
        auto message = boost::format("Failed to enter namespace %s: %s") % namespaceFile % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}

void enterMountNamespaceOfProcess(pid_t pid) {
    {
        auto file = boost::format("/proc/%s/ns/mnt") % pid;
        enterNamespace(file.str());
    }
}

void enterPidNamespaceOfProcess(pid_t pid) {
    {
        auto file = boost::format("/proc/%s/ns/pid") % pid;
        enterNamespace(file.str());
    }
}

/**
 * Find the mount root and mount point of a cgroup subsystem by parsing the [procPrefixDir]/proc/[pid]/mountinfo file.
 * For details about the syntax of such file, please refer to the proc(5) man page.
 * For details about cgroup subsystems belonging to different namespaces, please refer to the cgroup_namespaces(7) man page.
 */
std::tuple<boost::filesystem::path, boost::filesystem::path> findSubsystemMountPaths(const std::string& subsystemName,
        const boost::filesystem::path& procPrefixDir, const pid_t pid) {
    auto mountinfoPath = boost::filesystem::path(procPrefixDir / "proc" / std::to_string(pid) / "mountinfo");
    hook::logMessage(boost::format("Parsing %s for \"%s\" cgroup subsystem mount paths") % mountinfoPath % subsystemName,
                        libsarus::LogLevel::DEBUG);

    auto mountinfoText = filesystem::readFile(mountinfoPath);
    auto mountinfoLines = std::vector<std::string>{};
    boost::split(mountinfoLines, mountinfoText, boost::is_any_of("\n"));

    for (const auto& line : mountinfoLines) {
        auto tokens = std::vector<std::string>{};
        boost::split(tokens, line, boost::is_any_of(" "));

        if (tokens.size() < 10 ) {
            continue;
        }

        auto mountRoot      = tokens[3];
        auto mountPoint     = tokens[4];
        auto filesystemType = tokens.cend()[-3];
        auto superOptions   = tokens.cend()[-1];

        if (mountRoot.empty() || mountPoint.empty() || filesystemType.empty() || superOptions.empty()) {
            continue;
        }
        if (filesystemType != "cgroup") {
            continue;
        }
        if (superOptions.find(subsystemName) == std::string::npos) {
            continue;
        }
        if (boost::starts_with(mountRoot, "/..")) {
            auto message = boost::format("\"%s\" cgroup subsystem mount at %s belongs to a parent cgroup namespace")
                                         % subsystemName % mountPoint;
            SARUS_THROW_ERROR(message.str());
        }

        hook::logMessage(boost::format("Found \"%s\" cgroup subsystem mount root: %s") % subsystemName % mountRoot,
                            libsarus::LogLevel::DEBUG);
        hook::logMessage(boost::format("Found \"%s\" cgroup subsystem mount point: %s") % subsystemName % mountPoint,
                            libsarus::LogLevel::DEBUG);
        return std::tuple<boost::filesystem::path, boost::filesystem::path>{mountRoot, mountPoint};
    }

    auto message = boost::format("Could not find \"%s\" cgroup subsystem mount point within %s") % subsystemName % mountinfoPath;
    SARUS_THROW_ERROR(message.str());
}

/**
 * Find the pathname of a given control group to which a process belongs by parsing the [procPrefixDir]/proc/[pid]/cgroup file.
 * For details about the syntax of such file, please refer to the cgroups(7) man page.
 * For details about cgroup hierarchies rooted in different namespaces, please refer to the cgroup_namespaces(7) man page.
 * The returned path is relative to the mount point of the requested subsystem hierarchy.
 */
boost::filesystem::path findCgroupPathInHierarchy(const std::string& subsystemName, const boost::filesystem::path& procPrefixDir,
        const boost::filesystem::path& subsystemMountRoot, const pid_t pid) {
    auto procFilePath = boost::filesystem::path(procPrefixDir / "proc" / std::to_string(pid) / "cgroup");
    hook::logMessage(boost::format("Parsing %s for \"%s\" cgroup path relative to hierarchy mount point") % procFilePath % subsystemName,
                        libsarus::LogLevel::DEBUG);

    auto cgroupPath = boost::filesystem::path("/");
    auto procFileText = filesystem::readFile(procFilePath);
    auto procFileLines = std::vector<std::string>{};
    boost::split(procFileLines, procFileText, boost::is_any_of("\n"));

    for (const auto& line : procFileLines) {
        auto tokens = std::vector<std::string>{};
        boost::split(tokens, line, boost::is_any_of(":"));

        auto controllers   = tokens[1];
        auto cgroupPathStr = tokens[2];

        if (controllers.empty() || cgroupPathStr.empty()) {
            continue;
        }
        if (controllers.find(subsystemName) == std::string::npos) {
            continue;
        }
        if (boost::starts_with(cgroupPathStr, "/..")) {
            auto message = boost::format("\"%s\" cgroup hierarchy for process %d is rooted in another cgroup namespace")
                                         % subsystemName % pid;
            SARUS_THROW_ERROR(message.str());
        }
        if (subsystemMountRoot.string() != "/" && boost::starts_with(cgroupPathStr, subsystemMountRoot.string())) {
            cgroupPath /= boost::filesystem::relative(cgroupPathStr, subsystemMountRoot);
        }
        else {
            cgroupPath = boost::filesystem::path(cgroupPathStr);
        }

        hook::logMessage(boost::format("Found \"%s\" cgroup relative path for process %d: %s")
                                          % subsystemName % pid % cgroupPath,
                            libsarus::LogLevel::DEBUG);
        return boost::filesystem::path(cgroupPath);
    }

    auto message = boost::format("Could not find \"%s\" cgroup relative path for process %d within %s")
                                 % subsystemName % pid % procFilePath;
    SARUS_THROW_ERROR(message.str());
}

// Find the absolute path of a cgroup given a subsystem name, a prefix path for the location of a /proc filesystem and a pid
boost::filesystem::path findCgroupPath(const std::string& subsystemName, const boost::filesystem::path& procPrefixDir, const pid_t pid) {
    hook::logMessage(boost::format("Searching for cgroup \"%s\" subsystem under %s for process %d")
                        % subsystemName % procPrefixDir % pid, libsarus::LogLevel::DEBUG);

    boost::filesystem::path subsystemMountRoot;
    boost::filesystem::path subsystemMountPoint;
    std::tie(subsystemMountRoot, subsystemMountPoint) = findSubsystemMountPaths(subsystemName, procPrefixDir, pid);
    auto cgroupRelativePath = findCgroupPathInHierarchy(subsystemName, procPrefixDir, subsystemMountRoot, pid);

    auto cgroupPath = subsystemMountPoint / cgroupRelativePath;

    if (!boost::filesystem::exists(cgroupPath)) {
        auto message = boost::format("Found cgroups \"%s\" subsystem for process %d in %s, but directory doesn't exist")
            % subsystemName % pid % cgroupPath;
        SARUS_THROW_ERROR(message.str());
    }

    hook::logMessage(boost::format("Found cgroups \"%s\" subsystem for process %d in %s") % subsystemName  % pid % cgroupPath,
            libsarus::LogLevel::DEBUG);
    return cgroupPath;
}

/*
 * Whitelist a device for read/write access within a given cgroup
 * For reference about the involved files and syntax, check the following resource:
 * - https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v1/devices.html
 */
void whitelistDeviceInCgroup(const boost::filesystem::path& cgroupPath, const boost::filesystem::path& deviceFile) {
    hook::logMessage(boost::format("Whitelisting device %s for rw access in cgroup %s") % deviceFile % cgroupPath,
            libsarus::LogLevel::DEBUG);

    char deviceType;
    try {
        deviceType = filesystem::getDeviceType(deviceFile);
    }
    catch(const libsarus::Error& e) {
        auto message = boost::format("Failed to whitelist %s: not a valid device file") % deviceFile;
        SARUS_RETHROW_ERROR(e, message.str());
    }

    auto deviceID = filesystem::getDeviceID(deviceFile);
    auto entry = boost::format("%c %u:%u rw") % deviceType % major(deviceID) % minor(deviceID);
    hook::logMessage(boost::format("Whitelist entry: %s") % entry.str(), libsarus::LogLevel::DEBUG);

    auto allowFile = cgroupPath / "devices.allow";
    filesystem::writeTextFile(entry.str(), allowFile, std::ios_base::app);

    hook::logMessage(boost::format("Successfully whitelisted device %s for rw access") % deviceFile,
            libsarus::LogLevel::DEBUG);
}

void switchToUnprivilegedProcess(const uid_t targetUid, const gid_t targetGid) {
    // drop all capabilities
    // go through capability zero, one, two, ... until prctl() fails
    // because we went beyond the last valid capability
    for(int capIdx = 0; ; capIdx++) {
        if (prctl(PR_CAPBSET_DROP, capIdx, 0, 0, 0) != 0) {
            if(errno == EINVAL) {
                break; // reached end of valid capabilities
            }
            else {
                auto message = boost::format("Failed to prctl(PR_CAPBSET_DROP, %d, 0, 0, 0): %s")
                    % capIdx % strerror(errno);
                SARUS_THROW_ERROR(message.str());
            }
        }
    }

    // drop supplementary groups (if any)
    if(setgroups(0, NULL) != 0) {
        auto message = boost::format("Failed to setgroups(0, NULL): %s") % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    // change to user's gid
    if(setresgid(targetGid, targetGid, targetGid) != 0) {
        auto message = boost::format("Failed to setresgid(%1%, %1%, %1%): %2%") % targetGid % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    // change to user's uid
    if(setresuid(targetUid, targetUid, targetUid) != 0) {
        auto message = boost::format("Failed to setresuid(%1%, %1%, %1%): %2%") % targetUid % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    // set NoNewPrivs
    if(prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        auto message = boost::format("Failed to prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0): %s") % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}

std::tuple<unsigned int, unsigned int> parseLibcVersionFromLddOutput(const std::string& lddOutput) {
    auto outputLines = std::vector<std::string>();
    boost::split(outputLines, lddOutput, boost::is_any_of("\n"));
    boost::cmatch matches;
    boost::regex re("^ldd \\(.*\\) (\\d+)\\.(\\d+)?$");
    if (boost::regex_match(outputLines[0].c_str(), matches, re)) {
        return std::tuple<unsigned int, unsigned int>{
            std::stoi(matches[1]),
            std::stoi(matches[2])
        };
    }
    else {
        auto message = boost::format("Failed to parse glibc version from ldd output head:\n%s") % outputLines[0];
        SARUS_THROW_ERROR(message.str());
    }
}

void logMessage(const boost::format& message, libsarus::LogLevel level, std::ostream& out, std::ostream& err) {
    hook::logMessage(message.str(), level, out, err);
}

void logMessage(const std::string& message, libsarus::LogLevel level, std::ostream& out, std::ostream& err) {
    auto subsystemName = "hook";
    libsarus::Logger::getInstance().log(message, subsystemName, level, out, err);
}

}}
