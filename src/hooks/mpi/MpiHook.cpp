/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "hooks/mpi/MpiHook.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <rapidjson/document.h>

#include "libsarus/Logger.hpp"
#include "libsarus/Error.hpp"
#include "libsarus/Utility.hpp"
#include "SharedLibrary.hpp"

namespace sarus {
namespace hooks {
namespace mpi {

MpiHook::MpiHook() {
    log("Initializing hook", libsarus::LogLevel::INFO);

    containerState = libsarus::hook::parseStateOfContainerFromStdin();
    parseConfigJSONOfBundle();
    parseEnvironmentVariables();
    log("Getting list of shared libs from the container's dynamic linker cache", libsarus::LogLevel::DEBUG);
    auto containerLibPaths = libsarus::sharedlibs::getListFromDynamicLinker(ldconfig, rootfsDir);
    for (const auto& p : containerLibPaths){
        if ( !boost::filesystem::exists(rootfsDir / libsarus::filesystem::realpathWithinRootfs(rootfsDir, p)) ) {
            auto message = boost::format("Container library %s has an entry in the dynamic linker cache"
                                         " but does not exist or is a broken symlink in the container's"
                                         " filesystem. Skipping...") % p;
            log(message, libsarus::LogLevel::DEBUG);
            continue;
        }
        containerLibs.push_back(SharedLibrary(p, rootfsDir));
    }
    // Map Libraries
    hostToContainerMpiLibs = mapHostTocontainerLibs(hostMpiLibs, containerLibs);
    hostToContainerDependencyLibs = mapHostTocontainerLibs(hostDepLibs, containerLibs);

    log("Successfully initialized hook", libsarus::LogLevel::INFO);
}

void MpiHook::activateMpiSupport() {
    log("Activating MPI support", libsarus::LogLevel::INFO);

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
    injectHostLibraries(hostMpiLibs, hostToContainerMpiLibs, abiCompatibilityCheckerType);
    injectHostLibraries(hostDepLibs, hostToContainerDependencyLibs, "dependencies");
    performBindMounts();
    libsarus::process::executeCommand(ldconfig.string() + " -r " + rootfsDir.string()); // update container's dynamic linker

    log("Successfully activated MPI support", libsarus::LogLevel::INFO);
}

void MpiHook::parseConfigJSONOfBundle() {
    log("Parsing bundle's config.json", libsarus::LogLevel::INFO);

    auto json = libsarus::json::read(containerState.bundle() / "config.json");

    libsarus::hook::applyLoggingConfigIfAvailable(json);

    auto root = boost::filesystem::path{ json["root"]["path"].GetString() };
    if(root.is_absolute()) {
        rootfsDir = root;
    }
    else {
        rootfsDir = containerState.bundle() / root;
    }

    uid_t uidOfUser = json["process"]["user"]["uid"].GetInt();
    gid_t gidOfUser = json["process"]["user"]["gid"].GetInt();
    userIdentity = libsarus::UserIdentity(uidOfUser, gidOfUser, {});

    log("Successfully parsed bundle's config.json", libsarus::LogLevel::INFO);
}

void MpiHook::parseEnvironmentVariables() {
    log("Parsing environment variables", libsarus::LogLevel::INFO);

    ldconfig = libsarus::environment::getVariable("LDCONFIG_PATH");

    auto hostMpiLibsColonSeparated = libsarus::environment::getVariable("MPI_LIBS");
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

    AbiCheckerFactory checkerFactory;
    if((p = getenv("MPI_COMPATIBILITY_TYPE")) != nullptr && 
        checkerFactory.getCheckerTypes().find(std::string(p)) != checkerFactory.getCheckerTypes().end()) {
        abiCompatibilityCheckerType = std::string(p);
    }

    if ((p = getenv("HOOK_ROOTLESS")) != nullptr) {
        rootless = (boost::algorithm::to_upper_copy(std::string(p)) == std::string("TRUE"));
    }

    log("Successfully parsed environment variables", libsarus::LogLevel::INFO);
}

MpiHook::HostToContainerLibsMap MpiHook::mapHostTocontainerLibs(const std::vector<SharedLibrary>& hostLibs,
                                                                const std::vector<SharedLibrary>& containerLibs) const {
    log("Mapping host's shared libs to container's shared libs",
        libsarus::LogLevel::INFO);

    auto map = HostToContainerLibsMap{};

    for(const auto& hostLib : hostLibs) {
        for(const auto& containerLib : containerLibs) {
            if(hostLib.getLinkerName() == containerLib.getLinkerName()) {
                map[hostLib.getPath()].push_back(containerLib);

                auto message = boost::format("Found mapping: %s (host) -> %s (container)")
                               % hostLib.getPath() % containerLib.getPath();
                log(message, libsarus::LogLevel::DEBUG);
            }
        }
    }

    log("Successfully mapped host's shared libs to container's shared libs",
        libsarus::LogLevel::INFO);
    return map;
}

void MpiHook::checkHostMpiLibrariesHaveAbiVersion() const {
    log("Checking that host's MPI shared libs have ABI version",
        libsarus::LogLevel::INFO);

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
        libsarus::LogLevel::INFO);
}

void MpiHook::checkContainerMpiLibrariesHaveAbiVersion() const {
    log("Checking that container's MPI shared libs have ABI version",
        libsarus::LogLevel::INFO);

    for(const auto& entry : hostToContainerMpiLibs) {
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
        libsarus::LogLevel::INFO);
}

void MpiHook::checkHostContainerAbiCompatibility(const HostToContainerLibsMap& hostToContainerLibs) const {
    log("Checking shared libs ABI compatibility (host -> container)",
        libsarus::LogLevel::INFO);

    auto abiCompatibilityChecker{AbiCheckerFactory{}.create(abiCompatibilityCheckerType)};
    for(const auto& entry : hostToContainerLibs) {
        const auto& hostLib = SharedLibrary(entry.first);
        for(const auto& containerLib : entry.second) {
            try{
                abiCompatibilityChecker->check(hostLib, containerLib);
                auto value = abiCompatibilityChecker->check(hostLib, containerLib);
                if(!value.second && value.second) {
                    log(value.second.get(), libsarus::LogLevel::WARN);
                }
            } catch (const libsarus::Error&e ) {
                SARUS_THROW_ERROR(e.what());
            }
        }
    }

    log("Successfully checked shared libs ABI compatibility (host -> container)",
        libsarus::LogLevel::INFO);
}

void MpiHook::injectHostLibraries(const std::vector<SharedLibrary>& hostLibs,
                                  const HostToContainerLibsMap& hostToContainerLibs,
                                  const std::string& checkerType ) const {
    log("Injecting host's shared libs", libsarus::LogLevel::INFO);

    AbiCheckerFactory checkerFactory;
    for(const auto& lib : hostLibs) {
        injectHostLibrary(lib, hostToContainerLibs, std::move(checkerFactory.create(checkerType)));
    }

    log("Successfully injected host's shared libs", libsarus::LogLevel::INFO);
}

void MpiHook::injectHostLibrary(const SharedLibrary& hostLib,
                                const HostToContainerLibsMap& hostToContainerLibs,
                                std::unique_ptr<AbiCompatibilityChecker> abiCompatibilityChecker) const {
    log(boost::format{"Injecting host's shared lib %s"} % hostLib.getPath(), libsarus::LogLevel::DEBUG);

    const auto it = hostToContainerLibs.find(hostLib.getPath());
    if (it == hostToContainerLibs.cend()) {
        log(boost::format{"no corresponding libs in container => bind mount (%s) into /lib"} % hostLib.getPath(), libsarus::LogLevel::DEBUG);
        auto containerLib = "/lib" / hostLib.getPath().filename();
        libsarus::mount::validatedBindMount(hostLib.getPath(), containerLib, userIdentity, rootfsDir, 0, rootless);
        createSymlinksInDynamicLinkerDefaultSearchDirs(containerLib, hostLib.getPath().filename(), false);
        return;
    }
    // So, the container has at least one version of the host lib.
    // Let's pick the best candidate version to see how to proceed.
    const SharedLibrary bestCandidateLib = hostLib.pickNewestAbiCompatibleLibrary(it->second);
    log(boost::format{"for host lib %s, the best candidate lib in container is %s"} % hostLib.getPath() % bestCandidateLib.getPath(), libsarus::LogLevel::DEBUG);
    bool containerHasLibsWithIncompatibleVersion = containerHasIncompatibleLibraryVersion(hostLib, it->second);

    auto areCompatible{abiCompatibilityChecker->check(hostLib, bestCandidateLib)};
    if(areCompatible.second == boost::none) {
        log(boost::format{"abi-compatible => bind mount host lib (%s) on top of container lib (%s) (i.e. override)"} % hostLib.getPath() % bestCandidateLib.getPath(), libsarus::LogLevel::DEBUG);
        libsarus::mount::validatedBindMount(hostLib.getPath(), bestCandidateLib.getPath(), userIdentity, rootfsDir, 0, rootless);
        createSymlinksInDynamicLinkerDefaultSearchDirs(bestCandidateLib.getPath(), hostLib.getPath().filename(), containerHasLibsWithIncompatibleVersion);
        log("Successfully injected host's shared lib", libsarus::LogLevel::DEBUG);
        return;
    }
    log(areCompatible.second.get(), libsarus::LogLevel::INFO);
    auto containerLib = "/lib" / hostLib.getPath().filename();
    libsarus::mount::validatedBindMount(hostLib.getPath(), containerLib, userIdentity, rootfsDir, 0, rootless);
    if (areCompatible.first) {
        createSymlinksInDynamicLinkerDefaultSearchDirs(containerLib, hostLib.getPath().filename(), containerHasLibsWithIncompatibleVersion);
    } else {
        createSymlinksInDynamicLinkerDefaultSearchDirs(containerLib, hostLib.getPath().filename(), true);
    }
    log("Successfully injected host's shared lib", libsarus::LogLevel::DEBUG);
}

bool MpiHook::containerHasIncompatibleLibraryVersion(const SharedLibrary& hostLib, const std::vector<SharedLibrary>& containerLibraries) const{
    for (const auto& containerLib : containerLibraries) {
        if (containerLib.hasMajorVersion() && !areFullAbiCompatible(hostLib, containerLib)){
            return true;
        }
    }
    return false;
}

void MpiHook::performBindMounts() const {
    log("Performing bind mounts (configured through hook's environment variable BIND_MOUNTS)",
        libsarus::LogLevel::INFO);
    auto devicesCgroupPath = boost::filesystem::path{};

    for(const auto& mount : bindMounts) {
        libsarus::mount::validatedBindMount(mount, mount, userIdentity, rootfsDir, 0, rootless);

        if (libsarus::filesystem::isDeviceFile(mount)) {
            if (devicesCgroupPath.empty()) {
                devicesCgroupPath = libsarus::hook::findCgroupPath("devices", "/", containerState.pid());
            }

            libsarus::hook::whitelistDeviceInCgroup(devicesCgroupPath, mount);
        }
    }

    log("Successfully performed bind mounts", libsarus::LogLevel::INFO);
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
    auto libName = libsarus::sharedlibs::getLinkerName(linkFilename);
    auto linkNames = std::vector<std::string> { libName.string() };
    for(const auto& versionNumber : libsarus::sharedlibs::parseAbi(linkFilename)) {
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
                log(message, libsarus::LogLevel::DEBUG);
            }
        }
    }

    // Let's create symlinks in /lib and /lib64
    auto linkerDefaultSearchDirs = std::vector<boost::filesystem::path> {"/lib", "/lib64"};
    for (const auto& dir: linkerDefaultSearchDirs) {
        auto searchDir = rootfsDir / dir;
        libsarus::filesystem::createFoldersIfNecessary(searchDir);

        // prevent writing as root where we are not allowed to
        if (!libsarus::mount::isPathOnAllowedDevice(searchDir, rootfsDir)) {
            log(boost::format("The hook is not allowed to write to %s. Ignoring symlinks creation in this path.") % searchDir, libsarus::LogLevel::WARN);
            continue;
        }

        for (const auto& linkName : linkNames) {
            auto realLink = libsarus::filesystem::realpathWithinRootfs(rootfsDir, dir / linkName);
            auto realTarget = libsarus::filesystem::realpathWithinRootfs(rootfsDir, target);
            bool linkIsTarget = (realLink == realTarget);
            bool preserveLink = (linkName == libName && preserveRootLink && rootLinkExists);
            if (linkIsTarget || preserveLink) {
                continue;
            }

            auto link = searchDir / linkName;
            boost::filesystem::remove(link);
            boost::filesystem::create_symlink(target, link);

            auto message = boost::format("Created symlink in container %s -> %s") % link % target;
            log(message, libsarus::LogLevel::DEBUG);
        }
    }
}

void MpiHook::log(const std::string& message, libsarus::LogLevel level) const {
    libsarus::Logger::getInstance().log(message, "MPI hook", level);
}

void MpiHook::log(const boost::format& message, libsarus::LogLevel level) const {
    libsarus::Logger::getInstance().log(message.str(), "MPI hook", level);
}

}}} // namespace
