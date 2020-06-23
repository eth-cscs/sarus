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
#include "SharedLibrary.hpp"

namespace sarus {
namespace hooks {
namespace mpi {

MpiHook::MpiHook() {
    log("Initializing hook", sarus::common::LogLevel::INFO);

    std::tie(bundleDir, pidOfContainer) = hooks::common::utility::parseStateOfContainerFromStdin();
    sarus::hooks::common::utility::enterNamespacesOfProcess(pidOfContainer);
    parseConfigJSONOfBundle();
    parseEnvironmentVariables();
    // Load Container Libraries
    auto containerLibPaths = sarus::common::getSharedLibsFromDynamicLinker(ldconfig, rootfsDir);
    for (const auto& p : containerLibPaths){
        containerLibs.push_back(SharedLibrary(p, rootfsDir));
    }
    // Map Libraries
    hostToContainerMpiLibs = mapHostTocontainerLibs(hostMpiLibs, containerLibs);
    hostToContainerDependencyLibs = mapHostTocontainerLibs(hostDepLibs, containerLibs);

    log("Successfully initialized hook", sarus::common::LogLevel::INFO);
}

void MpiHook::activateMpiSupport() {
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
    injectHostLibraries(hostDepLibs, hostToContainerDependencyLibs);
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

    log("Successfully parsed bundle's config.json", sarus::common::LogLevel::INFO);
}

void MpiHook::parseEnvironmentVariables() {
    log("Parsing environment variables", sarus::common::LogLevel::INFO);

    ldconfig = sarus::common::getEnvironmentVariable("LDCONFIG_PATH");

    auto hostMpiLibsColonSeparated = sarus::common::getEnvironmentVariable("MPI_LIBS");
    if(hostMpiLibsColonSeparated.empty()) {
        SARUS_THROW_ERROR("The environment variable MPI_LIBS is expected to be a non-empty colon-separated list of paths");
    }
    std::vector<boost::filesystem::path> mpiPaths;
    boost::split(mpiPaths, hostMpiLibsColonSeparated, boost::is_any_of(":"), boost::token_compress_on);
    for (const auto& p : mpiPaths){
        hostMpiLibs.push_back(SharedLibrary(p));
    }

    char* p;
    if((p = getenv("MPI_DEPENDENCY_LIBS")) != nullptr && std::strcmp(p, "") != 0) {
        std::vector<boost::filesystem::path> depPaths;
        boost::split(depPaths, p, boost::is_any_of(":"), boost::token_compress_on);
        for (const auto& p : depPaths){
            hostDepLibs.push_back(SharedLibrary(p));
        }
    }

    if((p = getenv("BIND_MOUNTS")) != nullptr && std::strcmp(p, "") != 0) {
        boost::split(bindMounts, p, boost::is_any_of(":"), boost::token_compress_on);
    }

    log("Successfully parsed environment variables", sarus::common::LogLevel::INFO);
}

MpiHook::HostToContainerLibsMap MpiHook::mapHostTocontainerLibs(const std::vector<SharedLibrary>& hostLibs,
                                                                const std::vector<SharedLibrary>& containerLibs) const {
    log("Mapping host's shared libs to container's shared libs",
        sarus::common::LogLevel::INFO);

    auto map = HostToContainerLibsMap{};

    for(const auto& hostLib : hostLibs) {
        for(const auto& containerLib : containerLibs) {
            if(hostLib.getLinkerName() == containerLib.getLinkerName()) {
                map[hostLib.getPath()].push_back(containerLib);

                auto message = boost::format("Found mapping: %s (host) -> %s (container)")
                               % hostLib.getPath() % containerLib.getPath();
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

    for (const auto& lib : hostMpiLibs) {
        if (!lib.hasMajorVersion()) {
            auto message = boost::format(
                "The host's MPI libraries (configured through the env variable MPI_LIBS)"
                " must have at least the MAJOR ABI number, e.g. libmpi.so.<MAJOR>."
                " Only then can the compatibility between host and container MPI libraries be checked."
                " Found host's MPI library %s."
                " Please contact your system administrator to solve this issue."
            ) % lib.getPath();
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
            if(lib.hasMajorVersion()) {
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
            ) % entry.first;
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
        const auto& hostLib = SharedLibrary(entry.first);
        for(const auto& containerLib : entry.second) {
            if (containerLib.isFullAbiCompatible(hostLib)){
                continue;
            }
            if (containerLib.isMajorAbiCompatible(hostLib)){
                auto message = boost::format("Partial ABI compatibility detected. Host's MPI library %s is older than"
                        " the container's MPI library %s. The hook will attempt to proceed with the library replacement."
                        " Be aware that applications are likely to fail if they use symbols which are only present in the container's library."
                        " More information available at https://sarus.readthedocs.io/en/stable/user/abi_compatibility.html")
                        % hostLib.getRealName()
                        % containerLib.getRealName();
                log(message, sarus::common::LogLevel::WARN);
            }
            else {
                auto message = boost::format(
                    "Failed to activate MPI support. Host's MPI library %s is not ABI"
                    " compatible with container's MPI library %s.")
                    % hostLib.getRealName()
                    % containerLib.getRealName();
                SARUS_THROW_ERROR(message.str());
            }
        }
    }

    log("Successfully checked shared libs ABI compatibility (host -> container)",
        sarus::common::LogLevel::INFO);
}

void MpiHook::injectHostLibraries(const std::vector<SharedLibrary>& hostLibs,
                                  const HostToContainerLibsMap& hostToContainerLibs) const {
    log("Injecting host's shared libs", sarus::common::LogLevel::INFO);

    for(const auto& lib : hostLibs) {
        injectHostLibrary(lib, hostToContainerLibs);
    }

    log("Successfully injected host's shared libs", sarus::common::LogLevel::INFO);
}

void MpiHook::injectHostLibrary(const SharedLibrary& hostLib,
                                const HostToContainerLibsMap& hostToContainerLibs) const {
    log(boost::format{"Injecting host's shared lib %s"} % hostLib.getPath(), sarus::common::LogLevel::DEBUG);

    const auto it = hostToContainerLibs.find(hostLib.getPath());
    if (it == hostToContainerDependencyLibs.cend()) {
        log(boost::format{"no corresponding libs in container => bind mount (%s) into /lib"} % hostLib.getPath(), sarus::common::LogLevel::DEBUG);
        sarus::common::createFileIfNecessary(rootfsDir / "lib" / hostLib.getPath().filename());
        auto containerLibReal = sarus::common::realpathWithinRootfs(rootfsDir, "/lib" / hostLib.getPath().filename());
        sarus::runtime::bindMount(hostLib.getPath(), rootfsDir / containerLibReal);
        createSymlinksInDynamicLinkerDefaultSearchDirs("/lib" / hostLib.getPath().filename(), hostLib.getPath().filename(), false);
        return;
    }
    // So, the container has at least one version of the host lib.
    // Let's pick the best candidate version to see how to proceed.
    const SharedLibrary bestCandidateLib = hostLib.pickNewestAbiCompatibleLibrary(it->second);
    log(boost::format{"for host lib %s, the best candidate lib in container is %s"} % hostLib.getPath() % bestCandidateLib.getPath(), sarus::common::LogLevel::DEBUG);
    bool containerHasLibsWithIncompatibleVersion = containerHasIncompatibleLibraryVersion(hostLib, it->second);

    if (bestCandidateLib.isFullAbiCompatible(hostLib)){
        // safe replacement, all good.
        log(boost::format{"abi-compatible => bind mount host lib (%s) on top of container lib (%s) (i.e. override)"} % hostLib.getPath() % bestCandidateLib.getPath(), sarus::common::LogLevel::DEBUG);
        auto containerLibReal = sarus::common::realpathWithinRootfs(rootfsDir, bestCandidateLib.getPath());
        sarus::runtime::bindMount(hostLib.getPath(), rootfsDir / containerLibReal);
        createSymlinksInDynamicLinkerDefaultSearchDirs(containerLibReal, hostLib.getPath().filename(), containerHasLibsWithIncompatibleVersion);
    }
    else if (bestCandidateLib.isMajorAbiCompatible(hostLib)){
        // risky replacement, issue warning.
        log(boost::format{"WARNING: container lib (%s) is major-only-abi-compatible => bind mount host lib (%s) into /lib"} % bestCandidateLib.getPath() % hostLib.getPath(), sarus::common::LogLevel::DEBUG);
        sarus::common::createFileIfNecessary(rootfsDir / "lib" / hostLib.getPath().filename());
        auto containerLibReal = sarus::common::realpathWithinRootfs(rootfsDir, "/lib" / hostLib.getPath().filename());
        sarus::runtime::bindMount(hostLib.getPath(), rootfsDir / containerLibReal);
        createSymlinksInDynamicLinkerDefaultSearchDirs("/lib" / hostLib.getPath().filename(), hostLib.getPath().filename(), containerHasLibsWithIncompatibleVersion);
    }
    else {
        // inject with warning
        // NOTE: This branch is only for MPI dependency libraries. MPI libraries compatibility was already checked before at checkHostContainerAbiCompatibility. Hint for future refactoring.
        log(boost::format{"WARNING: container lib (%s) is NOT abi-compatible => bind mount host lib (%s) into /lib "} % bestCandidateLib.getPath() % hostLib.getPath(), sarus::common::LogLevel::WARN);
        sarus::common::createFileIfNecessary(rootfsDir / "lib" / hostLib.getPath().filename());
        auto containerLibReal = sarus::common::realpathWithinRootfs(rootfsDir, "/lib" / hostLib.getPath().filename());
        sarus::runtime::bindMount(hostLib.getPath(), rootfsDir / containerLibReal);
        createSymlinksInDynamicLinkerDefaultSearchDirs("/lib" / hostLib.getPath().filename(), hostLib.getPath().filename(), true);
    }

    log("Successfully injected host's shared lib", sarus::common::LogLevel::DEBUG);
}

bool MpiHook::containerHasIncompatibleLibraryVersion(const SharedLibrary& hostLib, const std::vector<SharedLibrary>& containerLibraries) const{
    bool containerHasLibsWithIncompatibleVersion = false;
    for (const auto& containerLib : containerLibraries) {
        if (containerLib.hasMajorVersion() && !containerLib.isFullAbiCompatible(hostLib)){
            containerHasLibsWithIncompatibleVersion = true;
            break;
        }
    }
    return containerHasLibsWithIncompatibleVersion;
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
                                                             const boost::filesystem::path& linkFilename,
                                                             const bool preserveRootLink) const {
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
    //    A consequence is that the container's ldconfig will create an entry in /etc/ld.so.cache
    //    for libmpich_gnu_71.so.3, and not for libmpi.so.12. This could prevent the container's
    //    ld.so from dynamically linking MPI applications to libmpi.so.12, if libmpi.so.12 is not
    //    in one of the ld.so's default search paths.
    //
    // Some ldconfig/ld.so versions/builds only search in the default directories /lib or /lib64.
    // So, let's create symlinks to the library in both /lib and /lib64 to make sure that they
    // will be found.
    //
    // preserveRootLink:
    //      As explained above, this method helps you create also a chain of symlinks that go from your library version
    //      up to the root linkername link. (e.g. you inject libfoo.so.4.1 and you end up with links libfoo.so.4 and libfoo.so).
    //      When a new library is injejcted and there were already other versions of it in the container, it is safer to preserve
    //      the root linkername (libfoo.so) link if it was available. For example, if the container had libfoo.so -> libfoo.so.5 and
    //      you inject libfoo.so.4, you don't want to end up with libfoo.so -> libfoo.so.4 because it may break the container apps.
    //      You should note that the library being injected (configured in Sarus configuration) should've been compiled using sonames,
    //      not the linker names, to avoid breaking the injected library for the same reason stated above.
    auto libName = sarus::common::getSharedLibLinkerName(linkFilename);
    auto linkNames = std::vector<std::string> { libName.string() };
    for(const auto& versionNumber : sarus::common::parseSharedLibAbi(linkFilename)) {
        linkNames.push_back(linkNames.back() + "." + versionNumber);
    }

    // Preserve root links (when requested)
    bool rootLinkExists = false;
    if (preserveRootLink) {
        std::vector<boost::filesystem::path> commonPaths = {"/lib", "/lib64", "/usr/lib", "/usr/lib64"};
        for (const auto& p : commonPaths){
            auto link = rootfsDir / p / libName;
            if (boost::filesystem::is_symlink(link) || boost::filesystem::is_regular_file(link)) {
                rootLinkExists = true;
                auto message = boost::format("Will not write root symlinks for %s because %s exits") % libName % link;
                log(message, sarus::common::LogLevel::DEBUG);
            }
        }
    }

    // Let's create symlinks in /lib and /lib64
    auto linkerDefaultSearchDirs = std::vector<boost::filesystem::path> {"/lib", "/lib64"};
    for (const auto& dir: linkerDefaultSearchDirs) {
        for (const auto& linkName : linkNames) {
            bool linkIsTarget = (dir / linkName == target);
            bool preserveLink = (linkName == libName && preserveRootLink && rootLinkExists);
            if (linkIsTarget || preserveLink) {
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
