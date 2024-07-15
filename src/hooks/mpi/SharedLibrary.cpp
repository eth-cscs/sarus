/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include "libsarus/Utility.hpp"
#include "hooks/mpi/SharedLibrary.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <unordered_map>
#include <vector>

namespace sarus {
namespace hooks {
namespace mpi {

// Strictly speaking, the following are not exact comparisons for major and full ABI compatibity. 
// For example, missing major or minor numbers are interpreted as "0". The same applies to the 
// class member SharedLibrary::hasMajorVersion.
// Nevertheless, these adjustments have been made in order to take care of corner cases that were
// experienced in user applications and "documented" thanks to unit tests.

bool areMajorAbiCompatible(const SharedLibrary& host, const SharedLibrary& container) {
    return host.getLinkerName() == container.getLinkerName() &&
            host.getMajorVersion() == container.getMajorVersion();
}

bool areFullAbiCompatible(const SharedLibrary& host, const SharedLibrary& container) {
    return areMajorAbiCompatible(host, container) &&
        host.getMinorVersion() >= container.getMinorVersion();
}

bool areStrictlyAbiCompatible(const SharedLibrary& host, const SharedLibrary& container) {
    return areMajorAbiCompatible(host, container) &&
        host.getMinorVersion() == container.getMinorVersion();
}

SharedLibrary::SharedLibrary(const boost::filesystem::path& path, const boost::filesystem::path& rootDir) : path(path) {
    linkerName = libsarus::sharedlibs::getLinkerName(path).string();
    auto abi = libsarus::sharedlibs::resolveAbi(path, rootDir);
    if (abi.size() > 0)
        major = std::stoi(abi[0]);
    if (abi.size() > 1)
        minor = std::stoi(abi[1]);
    if (abi.size() > 2)
        patch = std::stoi(abi[2]);
    auto abiString = boost::algorithm::join(abi, ".");
    if (abiString.size() > 0)
        realName = linkerName + "." + abiString;
    else
        realName = linkerName;
}

bool SharedLibrary::hasMajorVersion() const {
    return realName != linkerName;
}

bool SharedLibrary::isFullAbiCompatible(const SharedLibrary& hostLibrary) const{
    return areFullAbiCompatible(hostLibrary, *this);
}

bool SharedLibrary::isMajorAbiCompatible(const SharedLibrary& hostLibrary) const{
    return areMajorAbiCompatible(hostLibrary, *this);
}

SharedLibrary SharedLibrary::pickNewestAbiCompatibleLibrary(const std::vector<SharedLibrary>& candidates) const{
    // Essentially return the newest of the libraries older or equal to me. Otherwise, the oldest of the libraries newer than me.
    if (!candidates.size()){
        SARUS_THROW_ERROR("pickNewestAbiCompatibleLibrary received no candidates to pick from");
    }
    if (candidates.size() == 1){
        return candidates[0];
    }

    // find the oldest
    const SharedLibrary* oldest = &candidates[0];
    for (auto& c : candidates){
        if (c.getRealName() == realName){
            // exact match!
            return c;
        }
        if (c.major < oldest->major || (c.major == oldest->major && c.minor <= oldest->minor)){
            if (oldest->major == major && c.major < major){
                // don't go to an older major
                continue;
            }
            oldest = &c;
        }
    }

    // find best >= oldest but <= this
    const SharedLibrary* best = oldest;
    for (auto& c : candidates){
        if ((c.major > best->major || (c.major == best->major && c.minor >= best->minor)) &&
            c.major <= major &&
            c.minor <= minor) {
                // c newer than best, older than me.
                if (c.major == major && c.minor == minor && c.patch < best->patch) {
                    // don't downgrade patch
                    continue;
                }
                best = &c;
        }
    }
    return *best;
}

std::string SharedLibrary::getLinkerName() const {
    return linkerName;
}

boost::filesystem::path SharedLibrary::getPath() const {
    return path;
}

std::string SharedLibrary::getRealName() const {
    return realName;
}

}}}