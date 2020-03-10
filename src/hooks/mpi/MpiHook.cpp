/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "hooks/mpi/MpiHook.hpp"

#include <vector>
#include <fstream>
#include <sstream>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

#include "common/Logger.hpp"
#include "common/Utility.hpp"
#include "hooks/common/Utility.hpp"
#include "runtime/mount_utilities.hpp"

namespace sarus {
namespace hooks {
namespace mpi {

MpiHook::MpiHook() {
    log("Initializing hook", sarus::common::LogLevel::INFO);

    std::tie(bundleDir, pidOfContainer) = hooks::common::utility::parseStateOfContainerFromStdin();
    sarus::hooks::common::utility::enterNamespacesOfProcess(pidOfContainer);
    parseConfigJSONOfBundle();
    if(!isHookEnabled) {
        log("Initialization interrupted (hook disabled)", sarus::common::LogLevel::INFO);
        return;
    }
    parseEnvironmentVariables();
    containerLibraries = sarus::common::getSharedLibsFromDynamicLinker(ldconfig, rootfsDir);
    hostToContainerMpiLibs = mapHostToContainerLibraries(hostMpiLibs, containerLibraries);
    hostToContainerDependencyLibs = mapHostToContainerLibraries(hostDependencyLibs, containerLibraries);

    log("Successfully initialized hook", sarus::common::LogLevel::INFO);
}

void MpiHook::activateMpiSupport() {
    if(!isHookEnabled) {
        log("Not activating MPI support (hook disabled)", sarus::common::LogLevel::INFO);
        return;
    }

    log("Activating MPI support", sarus::common::LogLevel::INFO);

    if(hostToContainerMpiLibs.empty()) {
        SARUS_THROW_ERROR("Failed to activate MPI support. No MPI libraries"
                          " found in the container. The container should be"
                          " configured to access the MPI libraries through"
                          " the dynamic linker. Hint: run 'ldconfig' when building"
                          " the container image to configure the dynamic linker.");
    }
    checkHostMpiLibrariesHaveAbiVersion();
    checkContainerMpiLibrariesHaveAbiVersion();
    checkHostContainerAbiCompatibility(hostToContainerMpiLibs);
    injectHostLibraries(hostMpiLibs, hostToContainerMpiLibs);
    injectHostLibraries(hostDependencyLibs, hostToContainerDependencyLibs);
    performBindMounts();
    sarus::common::executeCommand(ldconfig.string() + " -r " + rootfsDir.string()); // update container's dynamic linker

    log("Successfully activated MPI support", sarus::common::LogLevel::INFO);
}

void MpiHook::parseConfigJSONOfBundle() {
    log("Parsing bundle's config.json", sarus::common::LogLevel::INFO);

    auto json = sarus::common::readJSON(bundleDir / "config.json");

    hooks::common::utility::applyLoggingConfigIfAvailable(json);

    auto root = boost::filesystem::path{ json["root"]["path"].GetString() };
    if(root.is_absolute()) {
        rootfsDir = root;
    }
    else {
        rootfsDir = bundleDir / root;
    }

    if(json.HasMember("annotations")
       && json["annotations"].HasMember("com.hooks.mpi.enabled")
       && json["annotations"]["com.hooks.mpi.enabled"].GetString() == std::string{"true"}) {
        isHookEnabled = true;
    }

    log("Successfully parsed bundle's config.json", sarus::common::LogLevel::INFO);
}

void MpiHook::parseEnvironmentVariables() {
    log("Parsing environment variables", sarus::common::LogLevel::INFO);

    ldconfig = sarus::common::getEnvironmentVariable("LDCONFIG_PATH");

    auto hostMpiLibsColonSeparated = sarus::common::getEnvironmentVariable("MPI_LIBS");
    if(hostMpiLibsColonSeparated.empty()) {
        SARUS_THROW_ERROR("The environment variable MPI_LIBS is expected to be a non-empty colon-separated list of paths");
    }
    boost::split(hostMpiLibs, hostMpiLibsColonSeparated, boost::is_any_of(":"), boost::token_compress_on);

    char* p;
    if((p = getenv("MPI_DEPENDENCY_LIBS")) != nullptr && std::strcmp(p, "") != 0) {
        boost::split(hostDependencyLibs, p, boost::is_any_of(":"), boost::token_compress_on);
    }

    if((p = getenv("BIND_MOUNTS")) != nullptr && std::strcmp(p, "") != 0) {
        boost::split(bindMounts, p, boost::is_any_of(":"), boost::token_compress_on);
    }

    log("Successfully parsed environment variables", sarus::common::LogLevel::INFO);
}

MpiHook::HostToContainerLibsMap MpiHook::mapHostToContainerLibraries(const std::vector<boost::filesystem::path>& hostLibs,
                                                                     const std::vector<boost::filesystem::path>& containerLibs) const {
    log("Mapping host's shared libs to container's shared libs",
        sarus::common::LogLevel::INFO);

    auto map = HostToContainerLibsMap{};

    for(const auto& hostLib : hostLibs) {
        for(const auto& containerLib : containerLibs) {
            if(sarus::common::getSharedLibLinkerName(hostLib) == sarus::common::getSharedLibLinkerName(containerLib)) {
                map[hostLib].push_back(containerLib);

                auto message = boost::format("Found mapping: %s (host) -> %s (container)")
                    % hostLib % containerLib;
                log(message, sarus::common::LogLevel::DEBUG);
            }
        }
    }

    log("Successfully mapped host's shared libs to container's shared libs",
        sarus::common::LogLevel::INFO);

    return map;
}

void MpiHook::checkHostMpiLibrariesHaveAbiVersion() const {
    log("Checking that host's MPI shared libs have ABI version",
        sarus::common::LogLevel::INFO);

    for(const auto lib : hostMpiLibs) {
        auto version = sarus::common::resolveSharedLibAbi(lib);
        if(version.empty()) {
            auto message = boost::format(
                "The host's MPI libraries (configured through the env variable MPI_LIBS)"
                " must have at least the MAJOR ABI number, e.g. libmpi.so.<MAJOR>."
                " Only then can the compatibility between host and container MPI libraries be checked."
                " Found host's MPI library %s."
                " Please contact your system administrator to solve this issue."
            ) % lib;
            SARUS_THROW_ERROR(message.str());
        }
    }

    log("Successfully checked that host's MPI shared libs have ABI version",
        sarus::common::LogLevel::INFO);
}

void MpiHook::checkContainerMpiLibrariesHaveAbiVersion() const {
    log("Checking that container's MPI shared libs have ABI version",
        sarus::common::LogLevel::INFO);

    for(const auto entry : hostToContainerMpiLibs) {
        bool found = false;
        for(const auto& lib : entry.second) {
            auto version = sarus::common::resolveSharedLibAbi(lib);
            if(!version.empty()) {
                found = true;
                break;
            }
        }

        if(!found) {
            auto message = boost::format(
                "The container's MPI libraries (configured through ldconfig)"
                " must have at least the MAJOR ABI number, e.g. libmpi.so.<MAJOR>."
                " Only then can the compatibility between host and container MPI"
                " libraries be checked. Failed to find a proper %s in the container."
                " Please adapt your container to meet the ABI compatibility check criteria."
            ) % sarus::common::getSharedLibLinkerName(entry.first);
            SARUS_THROW_ERROR(message.str());
        }
    }

    log("Successfully checked that container's MPI shared libs have ABI version",
        sarus::common::LogLevel::INFO);
}

void MpiHook::checkHostContainerAbiCompatibility(const HostToContainerLibsMap& hostToContainerLibs) const {
    log("Checking shared libs ABI compatibility (host -> container)",
        sarus::common::LogLevel::INFO);

    for(const auto& entry : hostToContainerLibs) {
        for(const auto& containerLib : entry.second) {
            const auto& hostLib = entry.first;
            auto containerAbi = sarus::common::resolveSharedLibAbi(containerLib, rootfsDir);
            auto hostAbi = sarus::common::resolveSharedLibAbi(hostLib);

            if(!sarus::common::areAbiVersionsCompatible(hostAbi, containerAbi))  {
                auto message = boost::format(
                    "Failed to activate MPI support. Host's MPI library %s (ABI '%s') is not ABI"
                    " compatible with container's MPI library %s (ABI '%s').")
                    % hostLib % boost::algorithm::join(hostAbi, ".")
                    % containerLib % boost::algorithm::join(containerAbi, ".");
                SARUS_THROW_ERROR(message.str());
            }
        }
    }

    log("Successfully checked shared libs ABI compatibility (host -> container)",
        sarus::common::LogLevel::INFO);
}

void MpiHook::injectHostLibraries(const std::vector<boost::filesystem::path>& hostLibs,
                                  const HostToContainerLibsMap& hostToContainerLibs) const {
    log("Injecting host's shared libs", sarus::common::LogLevel::INFO);

    for(const auto& lib : hostLibs) {
        injectHostLibrary(lib, hostToContainerLibs);
    }

    log("Successfully injected host's shared libs", sarus::common::LogLevel::INFO);
}

void MpiHook::injectHostLibrary(const boost::filesystem::path& hostLib,
                                const HostToContainerLibsMap& hostToContainerLibs) const {
    log(boost::format{"Injecting host's shared lib %s"} % hostLib, sarus::common::LogLevel::DEBUG);

    auto it = hostToContainerLibs.find(hostLib);

    // no corresponding libs in container => bind mount into /lib
    if(it == hostToContainerDependencyLibs.cend()) {
        sarus::common::createFileIfNecessary(rootfsDir / "lib" / hostLib.filename());
        auto containerLibReal = sarus::common::realpathWithinRootfs(rootfsDir, "/lib" / hostLib.filename());
        sarus::runtime::bindMount(hostLib, rootfsDir / containerLibReal);
        createSymlinksInDynamicLinkerDefaultSearchDirs("/lib" / hostLib.filename(), hostLib.filename());
        return;
    }

    // for each corresponding lib in container
    for(const auto& containerLib : it->second) {
        auto hostAbi = sarus::common::resolveSharedLibAbi(hostLib);
        auto containerAbi = sarus::common::resolveSharedLibAbi(containerLib, rootfsDir);
        // abi-compatible => bind mount on top of it (override)
        if(sarus::common::areAbiVersionsCompatible(hostAbi, containerAbi)) {
            auto containerLibReal = sarus::common::realpathWithinRootfs(rootfsDir, containerLib);
            sarus::runtime::bindMount(hostLib, rootfsDir / containerLibReal);
            createSymlinksInDynamicLinkerDefaultSearchDirs(containerLibReal, hostLib.filename());
        }
        // not abi-compatible => bind mount into /lib
        else {
            sarus::common::createFileIfNecessary(rootfsDir / "lib" / hostLib.filename());
            auto containerLibReal = sarus::common::realpathWithinRootfs(rootfsDir, "/lib" / hostLib.filename());
            sarus::runtime::bindMount(hostLib, rootfsDir / containerLibReal);
            createSymlinksInDynamicLinkerDefaultSearchDirs("/lib" / hostLib.filename(), hostLib.filename());
        }
    }

    log("Successfully injected host's shared lib", sarus::common::LogLevel::DEBUG);
}

void MpiHook::performBindMounts() const {
    log("Performing bind mounts (configured through hook's environment variable BIND_MOUNTS)",
        sarus::common::LogLevel::INFO);

    for(const auto& mount : bindMounts) {
        if(boost::filesystem::is_directory(mount)) {
            sarus::common::createFoldersIfNecessary(rootfsDir / mount);
        }
        else {
            sarus::common::createFileIfNecessary(rootfsDir / mount);
        }
        sarus::runtime::bindMount(mount, rootfsDir / mount, MS_REC);
    }

    log("Successfully performed bind mounts", sarus::common::LogLevel::INFO);
}

void MpiHook::createSymlinksInDynamicLinkerDefaultSearchDirs(const boost::filesystem::path& target,
                                                             const boost::filesystem::path& linkFilename) const {
    // Generate symlinks to the library in the container's /lib and /lib64, to make sure that:
    //
    // 1. ldconfig will find the library in the container, because the symlink will be in
    //    one of ldconfig's default search directories.
    //
    // 2. ld.so will find the library regardless of the library's SONAME (ELF header entry),
    //    because the symlink will be in one of ld.so's default search paths.
    //
    //    This is important, because on some systems a library's SONAME (ELF header entry) might
    //    not correspond to the library's filename. E.g. on Cray CLE 7, the SONAME of
    //    /opt/cray/pe/mpt/7.7.9/gni/mpich-gnu-abi/7.1/lib/libmpi.so.12 is libmpich_gnu_71.so.3.
    //    A conseguence is that the container's ldconfig will create an entry in /etc/ld.so.cache
    //    for libmpich_gnu_71.so.3, and not for libmpi.so.12. This could prevent the container's
    //    ld.so from dynamically linking MPI applications to libmpi.so.12, if libmpi.so.12 is not
    //    in one of the ld.so's default search paths.
    //
    // Some ldconfig/ld.so versions/builds only search in the default directories /lib or /lib64.
    // So, let's create symlinks to the library in both /lib and /lib64 to make sure that they
    // will be found.

    auto libName = sarus::common::getSharedLibLinkerName(linkFilename);
    auto linkNames = std::vector<std::string> { libName.string() };
    for(const auto& versionNumber : sarus::common::parseSharedLibAbi(linkFilename)) {
        linkNames.push_back(linkNames.back() + "." + versionNumber);
    }

    auto linkerDefaultSearchDirs = std::vector<boost::filesystem::path> {"/lib", "/lib64"};
    for (const auto& dir: linkerDefaultSearchDirs) {
        for (const auto& linkName : linkNames) {
            if(dir / linkName == target) {
                continue;
            }
            auto link = rootfsDir / dir / linkName;
            sarus::common::createFoldersIfNecessary(link.parent_path());
            boost::filesystem::remove(link); // remove if already exists
            boost::filesystem::create_symlink(target, link);

            auto message = boost::format("Created symlink in container %s -> %s") % link % target;
            log(message, sarus::common::LogLevel::DEBUG);
        }
    }
}

void MpiHook::log(const std::string& message, sarus::common::LogLevel level) const {
    sarus::common::Logger::getInstance().log(message, "MPI hook", level);
}

void MpiHook::log(const boost::format& message, sarus::common::LogLevel level) const {
    sarus::common::Logger::getInstance().log(message.str(), "MPI hook", level);
}

}}} // namespace
