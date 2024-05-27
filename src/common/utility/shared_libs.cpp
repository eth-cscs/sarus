/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "shared_libs.hpp"

#include <sstream>

#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

#include "common/Error.hpp"
#include "common/utility/logging.hpp"
#include "common/utility/filesystem.hpp"
#include "common/utility/process.hpp"

/**
 * Utility functions for shared libraries 
 */

namespace sarus {
namespace common {

boost::filesystem::path getSharedLibLinkerName(const boost::filesystem::path& path) {
    auto extension = std::string{".so"};
    auto filename = path.filename().string();
    auto dot = std::find_end(filename.begin(), filename.end(), extension.cbegin(), extension.cend());

    if(dot == filename.cend() || (dot+3 != filename.cend() && *(dot+3) != '.')) {
        auto message = boost::format{"Failed to parse linker name from invalid library path '%s'"} % path;
        SARUS_THROW_ERROR(message.str());
    }

    filename.erase(dot+3, filename.end());
    return filename;
}

std::vector<boost::filesystem::path> getSharedLibsFromDynamicLinker(
    const boost::filesystem::path& ldconfigPath,
    const boost::filesystem::path& rootDir) {

    auto libraries = std::vector<boost::filesystem::path>{};
    auto command = boost::format("%s -r %s -p") % ldconfigPath.string() % rootDir.string();
    auto output = sarus::common::executeCommand(command.str());
    std::stringstream stream{output};
    std::string line;
    while(std::getline(stream, line)) {
        // Look for "arrow" separator to only parse lines containing library entries
        auto pos = line.rfind(" => ");
        if(pos == std::string::npos) {
            continue;
        }
        auto library = line.substr(pos + 4);
        libraries.push_back(library);
    }

    return libraries;
}

std::vector<std::string> parseSharedLibAbi(const boost::filesystem::path& lib) {
    if(!isSharedLib(lib)) {
        auto message = boost::format{"Cannot parse ABI version of '%s': not a shared library"} % lib;
        SARUS_THROW_ERROR(message.str());
    }

    auto name = lib.filename().string();
    auto extension = std::string{".so"};

    auto pos = std::find_end(name.cbegin(), name.cend(), extension.cbegin(), extension.cend());

    if(pos == name.cend()) {
        auto message = boost::format("Failed to get version numbers of library %s."
                                     " Expected a library with file extension '%s'.") % lib % extension;
        SARUS_THROW_ERROR(message.str());
    }

    if(pos+3 == name.cend()) {
        return {};
    }

    auto tokens = std::vector<std::string>{};
    auto versionString = std::string(pos+4, name.cend());
    boost::split(tokens, versionString, boost::is_any_of("."));

    return tokens;
}

std::vector<std::string> resolveSharedLibAbi(const boost::filesystem::path& lib,
                                             const boost::filesystem::path& rootDir) {
    if(!isSharedLib(lib)) {
        auto message = boost::format{"Cannot resolve ABI version of '%s': not a shared library"} % lib;
        SARUS_THROW_ERROR(message.str());
    }

    auto longestAbiSoFar = std::vector<std::string>{};

    auto traversedSymlinks = std::vector<boost::filesystem::path>{};
    auto libReal = appendPathsWithinRootfs(rootDir, "/", lib, &traversedSymlinks);
    auto pathsToProcess = std::move(traversedSymlinks);
    pathsToProcess.push_back(std::move(libReal));

    for(const auto& path : pathsToProcess) {
        if(!isSharedLib(path)) {
            // some traversed symlinks may not be library filenames,
            // e.g. with /lib -> /lib64
            continue;
        }
        if(getSharedLibLinkerName(path) != getSharedLibLinkerName(lib)) {
            // E.g. on Cray we could have:
            // mpich-gnu-abi/7.1/lib/libmpi.so.12 -> ../../../mpich-gnu/7.1/lib/libmpich_gnu_71.so.3.0.1
            // Let's ignore the symlink's target in this case
            auto message = boost::format("Failed to resolve ABI version of\n%s -> %s\nThe symlink"
                                        " and the target library have incompatible linker names. Assuming the symlink is correct.")
                                        % lib % path;
            logMessage(message.str(), LogLevel::DEBUG);
            continue;
        }

        auto abi = parseSharedLibAbi(path);

        const auto& shorter = abi.size() < longestAbiSoFar.size() ? abi : longestAbiSoFar;
        const auto& longer = abi.size() > longestAbiSoFar.size() ? abi : longestAbiSoFar;

        if(!std::equal(shorter.cbegin(), shorter.cend(), longer.cbegin())) {
            // Some vendors have symlinks with incompatible major versions. e.g.
            // libvdpau_nvidia.so.1 -> libvdpau_nvidia.so.440.33.01.
            // For these cases, we trust the vendor and resolve the Lib Abi to that of the symlink.
            auto message = boost::format("Failed to resolve ABI version of\n%s -> %s\nThe symlink filename"
                                        " and the target library have incompatible ABI versions. Assuming symlink is correct.")
                                        % lib % path;
            logMessage(message.str(), LogLevel::DEBUG);
            continue;
        }

        longestAbiSoFar = std::move(longer);
    }

    return longestAbiSoFar;
}

std::string getSharedLibSoname(const boost::filesystem::path& path, const boost::filesystem::path& readelfPath) {
    auto command = boost::format("%s -d %d") % readelfPath % path;
    auto output = sarus::common::executeCommand(command.str());

    std::stringstream stream{output};
    std::string line;
    while(std::getline(stream, line)) {
        boost::cmatch matches;
        boost::regex re("^.* \\(SONAME\\) +Library soname: \\[(.*)\\]$");
        if (boost::regex_match(line.c_str(), matches, re)) {
            return matches[1];
        }
    }

    auto message = boost::format("Failed to parse library soname from readelf output: %s") % output;
    SARUS_THROW_ERROR(message.str());
}

bool is64bitSharedLib(const boost::filesystem::path& path, const boost::filesystem::path& readelfPath) {
    boost::cmatch matches;
    boost::regex re("^ *Machine: +Advanced Micro Devices X86-64 *$");

    auto command = boost::format("%s -h %s") % readelfPath.string() % path.string();
    auto output = sarus::common::executeCommand(command.str());

    std::stringstream stream{output};
    std::string line;
    while(std::getline(stream, line)) {
        if (boost::regex_match(line.c_str(), matches, re)) {
            return true;
        }
    }

    return false;
}

} // namespace
} // namespace
