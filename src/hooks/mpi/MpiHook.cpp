/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "hooks/mpi/MpiHook.hpp"

#include <vector>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

#include "common/Utility.hpp"
#include "hooks/common/Utility.hpp"
#include "runtime/mount_utilities.hpp"

namespace sarus {
namespace hooks {
namespace mpi {


void MpiHook::activateMpiSupport() {
    std::tie(bundleDir, pidOfContainer) = hooks::common::utility::parseStateOfContainerFromStdin();
    sarus::hooks::common::utility::enterNamespacesOfProcess(pidOfContainer);
    parseConfigJSONOfBundle();
    if(!isHookEnabled) {
        return;
    }
    parseEnvironmentVariables();
    getContainerLibrariesFromDynamicLinker();
    replaceMpiLibrariesInContainer();
    mountDependencyLibrariesIntoContainer();
    performBindMounts();
    sarus::common::executeCommand("ldconfig -r" + rootfsDir.string()); // update container's dynamic linker
}

void MpiHook::parseConfigJSONOfBundle() {
    auto json = sarus::common::readJSON(bundleDir / "config.json");

    // get rootfs
    rootfsDir = bundleDir / json["root"]["path"].GetString();

    // get environment variables
    auto env = hooks::common::utility::parseEnvironmentVariablesFromOCIBundle(bundleDir);
    if(env["SARUS_MPI_HOOK"] == "1") {
        isHookEnabled = true;
    }
}

void MpiHook::parseEnvironmentVariables() {
    char* p;
    if((p = getenv("SARUS_MPI_LIBS")) == nullptr) {
        SARUS_THROW_ERROR("Environment doesn't contain variable SARUS_MPI_LIBS");
    }
    hostMpiLibs = parseListOfPaths(p);

    if((p = getenv("SARUS_MPI_DEPENDENCY_LIBS")) != nullptr) {
        hostMpiDependencyLibs = parseListOfPaths(p);
    }

    if((p = getenv("SARUS_MPI_BIND_MOUNTS")) != nullptr) {
        bindMounts = parseListOfPaths(p);
    }
}

std::vector<boost::filesystem::path> MpiHook::parseListOfPaths(const std::string& s) {
    auto list = std::vector<boost::filesystem::path>{};

    if(s.empty()) {
        return list;
    }

    auto pathBegin = s.cbegin();
    while(true) {
        auto pathEnd = std::find(pathBegin, s.cend(), ':');
        auto path = std::string(pathBegin, pathEnd);
        list.push_back(std::move(path));

        if(pathEnd == s.cend()) {
            break;
        }
        pathBegin = pathEnd + 1;
    }

    return list;
}

void MpiHook::getContainerLibrariesFromDynamicLinker() {
    auto output = sarus::common::executeCommand("ldconfig -r" + rootfsDir.string() + " -p");
    std::stringstream stream{output};
    std::string line;
    std::getline(stream, line); // drop first line of output (header)
    while(std::getline(stream, line)) {
        auto pos = line.rfind(" => ");
        auto library = line.substr(pos + 4);
        auto libraryRealpath = sarus::common::realpathWithinRootfs(rootfsDir, library);
        library = "/" + boost::filesystem::relative(libraryRealpath, rootfsDir).string();
        auto name = getStrippedLibraryName(library);
        containerLibraries[name] = library;
    }
}

void MpiHook::replaceMpiLibrariesInContainer() const {
    if(hostMpiLibs.empty()) {
        SARUS_THROW_ERROR("Failed to activate MPI support. No host MPI"
                            " libraries specified. Please contact the system"
                            " administrator to properly configure the MPI hook");
    }

    bool containerHasMpiLibrary = false;

    for(const auto& hostMpiLib : hostMpiLibs) {
        auto name = getStrippedLibraryName(hostMpiLib);
        auto it = containerLibraries.find(name);
        if(it != containerLibraries.cend()) {
            containerHasMpiLibrary = true;
            const auto& containerMpiLib = it->second;
            if(!areLibrariesABICompatible(hostMpiLib, containerMpiLib)) {
                auto message = boost::format(   "Failed to activate MPI support. Host's library %s is not ABI"
                                                " compatible with container's library %s") % hostMpiLib % containerMpiLib;
                SARUS_THROW_ERROR(message.str());
            }
            sarus::runtime::bindMount(hostMpiLib, rootfsDir / containerMpiLib);
            if (!isLibraryInDefaultLinkerDirectory(containerMpiLib)) {
                createSymlinksInDefaultLinkerDirectory(containerMpiLib);
            }
        }
    }

    if(!containerHasMpiLibrary) {
        SARUS_THROW_ERROR("Failed to activate MPI support. No MPI libraries"
                            " found in the container. The container should be"
                            " configured to access the MPI libraries through"
                            " the dynamic linker. Hint: run 'ldconfig' when building"
                            " the container image to configure the dynamic linker.");
    }
}

void MpiHook::mountDependencyLibrariesIntoContainer() const {
    for(const auto& hostLib : hostMpiDependencyLibs) {
        auto containerLib = rootfsDir / "usr/lib" / hostLib.filename();
        sarus::common::createFileIfNecessary(containerLib);
        sarus::runtime::bindMount(hostLib, containerLib);
    }
}

void MpiHook::performBindMounts() const {
    for(const auto& mount : bindMounts) {
        if(boost::filesystem::is_directory(mount)) {
            sarus::common::createFoldersIfNecessary(rootfsDir / mount);
        }
        else {
            sarus::common::createFileIfNecessary(rootfsDir / mount);
        }
        sarus::runtime::bindMount(mount, rootfsDir / mount, MS_REC);
    }
}

bool MpiHook::areLibrariesABICompatible(const boost::filesystem::path& hostLib, const boost::filesystem::path& containerLib) const {
    int hostMajor, hostMinor, containerMajor, containerMinor;
    std::tie(hostMajor, hostMinor, std::ignore) = getVersionNumbersOfLibrary(hostLib);
    std::tie(containerMajor, containerMinor, std::ignore) = getVersionNumbersOfLibrary(containerLib);

    if(hostMajor != containerMajor) {
        return false;
    }
    if(hostMinor < containerMinor) {
        return false;
    }

    return true;
}

bool MpiHook::isLibraryInDefaultLinkerDirectory(const boost::filesystem::path& lib) const {
    return lib.parent_path() == "/lib"
            || lib.parent_path() == "/lib64"
            || lib.parent_path() == "/usr/lib"
            || lib.parent_path() == "/usr/lib64";
}

void MpiHook::createSymlinksInDefaultLinkerDirectory(const boost::filesystem::path& lib) const {
    int versionMajor, versionMinor, versionPatch;
    std::tie(versionMajor, versionMinor, versionPatch) = getVersionNumbersOfLibrary(lib);

    // Some ldconfig implementations only check in the default directories /lib64 or /lib,
    // ignoring the other architecture option.
    // We create symlinks to MPI libraries in both locations to ensure they are found.
    // According to the file-hierarchy(7) man page, modern Linux distributions could implement
    // /lib and /lib64 as symlinks.
    // In order to be flexible with regard to container filesystems, we use the following strategy:
    //     - If /usr/lib64 does not exist, create it
    //     - If /lib64 does not exist, create it as symlink to /usr/lib
    //       (if it exists, it is either a proper directory or a symlink)
    //     - Repeat the steps above for /usr/lib and /lib
    //     - List /lib64 and /lib as MPI symlink paths

    // lib64 path
    auto usrLib64Path = boost::filesystem::path(rootfsDir / "/usr/lib64");
    auto lib64Path = boost::filesystem::path(rootfsDir / "/lib64");
    if (!boost::filesystem::exists(usrLib64Path)) {
        sarus::common::createFoldersIfNecessary(usrLib64Path);
    }
    if (!boost::filesystem::exists(lib64Path)) {
        boost::filesystem::create_symlink(usrLib64Path, lib64Path);
    }

    // lib path
    auto usrLibPath = boost::filesystem::path(rootfsDir / "/usr/lib");
    auto libPath = boost::filesystem::path(rootfsDir / "/lib");
    if (!boost::filesystem::exists(usrLibPath)) {
        sarus::common::createFoldersIfNecessary(usrLibPath);
    }
    if (!boost::filesystem::exists(libPath)) {
        boost::filesystem::create_symlink(usrLibPath, libPath);
    }

    std::vector<boost::filesystem::path> symlinkDirectories{lib64Path, libPath};

    auto libName = getStrippedLibraryName(lib) + ".so";
    std::vector<boost::format> symlinks;
    symlinks.push_back(boost::format(libName));
    symlinks.push_back(boost::format("%s.%d") % libName % versionMajor);
    symlinks.push_back(boost::format("%s.%d.%d") % libName % versionMajor % versionMinor);
    symlinks.push_back(boost::format("%s.%d.%d.%d") % libName % versionMajor % versionMinor % versionPatch);

    for (const auto& dir: symlinkDirectories) {
        for (const auto& link : symlinks) {
            boost::filesystem::create_symlink(lib, dir / link.str());
        }
    }
}

std::tuple<int, int, int> MpiHook::getVersionNumbersOfLibrary(const boost::filesystem::path& lib) const {
    auto name = lib.filename().string();

    boost::regex regex("lib.*\\.so(\\.[0-9]+)*");
    boost::cmatch matches;
    if(!boost::regex_match(name.c_str(), matches, regex)) {
        auto message = boost::format(   "Failed to get version numbers of library %s."
                                        " Library name is invalid, because the regex"
                                        " \"%s\" doesn't match.") % lib % regex;
        SARUS_THROW_ERROR(message.str());
    }

    int major = 0;
    int minor = 0;
    int patch = 0;
    auto pos = name.rfind(".so");

    // no numbers to parse
    if(name.cbegin() + pos + 3 == name.cend()) {
        return std::tuple<int, int, int>{major, minor, patch};
    }

    // parse major number
    auto majorBeg = name.cbegin() + pos + 4;
    auto majorEnd = std::find(majorBeg, name.cend(), '.');
    major = std::stoi(std::string(majorBeg, majorEnd));
    if(majorEnd == name.cend()) {
        return std::tuple<int, int, int>{major, minor, patch};
    }

    // parse minor number
    auto minorBeg = majorEnd + 1;
    auto minorEnd = std::find(minorBeg, name.cend(), '.');
    minor = std::stoi(std::string(minorBeg, minorEnd));
    if(minorEnd == name.cend()) {
        return std::tuple<int, int, int>{major, minor, patch};
    }

    // parse patch number
    auto patchBeg = minorEnd + 1;
    auto patchEnd = std::find(patchBeg, name.cend(), '.');
    patch = std::stoi(std::string(patchBeg, patchEnd));

    return std::tuple<int, int, int>{major, minor, patch};
}

std::string MpiHook::getStrippedLibraryName(const boost::filesystem::path& path) const {
    auto name = path.filename().string();
    auto pos = name.rfind(".so");
    return name.substr(0, pos);
}

}}} // namespace
