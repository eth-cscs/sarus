/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "hooks/glibc/GlibcHook.hpp"

#include <vector>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

#include "common/Utility.hpp"
#include "hooks/common/Utility.hpp"
#include "runtime/mount_utilities.hpp"

namespace sarus {
namespace hooks {
namespace glibc {

GlibcHook::GlibcHook() {
    logMessage("Initializing hook", sarus::common::LogLevel::INFO);

    std::tie(bundleDir, pidOfContainer) = hooks::common::utility::parseStateOfContainerFromStdin();
    sarus::hooks::common::utility::enterNamespacesOfProcess(pidOfContainer);
    parseConfigJSONOfBundle();
    parseEnvironmentVariables();

    logMessage("Successfully initialized hook", sarus::common::LogLevel::INFO);
}

void GlibcHook::injectGlibcLibrariesIfNecessary() {
    logMessage("Replacing container's glibc libraries", sarus::common::LogLevel::INFO);

    auto hostLibc = findLibc(hostLibraries);
    if(!hostLibc) {
        SARUS_THROW_ERROR(  "Failed to inject glibc libraries. Could not find the host's libc."
                            " Please contact the system administrator to properly configure the glibc hook");
    }
    if(!containerHasGlibc()) {
        logMessage("Not replacing glibc libraries (container doesn't have glibc)", sarus::common::LogLevel::INFO);
        return; // nothing to do
    }
    containerLibraries = get64bitContainerLibraries();
    auto containerLibc = findLibc(containerLibraries);
    if(!containerLibc) {
        logMessage("Not replacing glibc libraries (container doesn't have 64-bit libc)", sarus::common::LogLevel::INFO);
        return; // nothing to do (could be a 32-bit container without a 64-bit libc)
    }
    if(!containerGlibcHasToBeReplaced(*hostLibc, *containerLibc)) {
        logMessage("Not replacing glibc libraries (container's glibc is new enough)", sarus::common::LogLevel::INFO);
        return; // nothing to do
    }
    verifyThatHostAndContainerGlibcAreABICompatible(*hostLibc, *containerLibc);
    replaceGlibcLibrariesInContainer();

    logMessage("Successfully replaced glibc libraries", sarus::common::LogLevel::INFO);
}

void GlibcHook::parseConfigJSONOfBundle() {
    logMessage("Parsing bundle's config.json", sarus::common::LogLevel::INFO);

    auto json = sarus::common::readJSON(bundleDir / "config.json");

    hooks::common::utility::applyLoggingConfigIfAvailable(json);

    // get rootfs
    auto root = boost::filesystem::path{ json["root"]["path"].GetString() };
    if(root.is_absolute()) {
        rootfsDir = root;
    }
    else {
        rootfsDir = bundleDir / root;
    }

    logMessage("Successfully parsed bundle's config.json", sarus::common::LogLevel::INFO);
}

void GlibcHook::parseEnvironmentVariables() {
    logMessage("Parsing environment variables", sarus::common::LogLevel::INFO);

    ldconfigPath = sarus::common::getEnvironmentVariable("LDCONFIG_PATH");

    readelfPath = sarus::common::getEnvironmentVariable("READELF_PATH");

    auto hostLibrariesColonSeparated = sarus::common::getEnvironmentVariable("GLIBC_LIBS");
    boost::split(hostLibraries, hostLibrariesColonSeparated, boost::is_any_of(":"));

    logMessage("Successfully parsed environment variables", sarus::common::LogLevel::INFO);
}

bool GlibcHook::containerHasGlibc() const {
    // If the container has glibc (some systems such as Alpine Linux don't),
    // then we expect /etc/ld.so.cache to be in the container (it is generated
    // by ldconfig, which is part of glibc) 
    return boost::filesystem::is_regular_file(rootfsDir / "/etc/ld.so.cache");
}

std::vector<boost::filesystem::path> GlibcHook::get64bitContainerLibraries() const {
    auto isNot64bit = [this](const boost::filesystem::path& lib) {
        return !sarus::common::is64bitSharedLib(rootfsDir / sarus::common::realpathWithinRootfs(rootfsDir, lib), readelfPath);
    };
    auto libs = sarus::common::getSharedLibsFromDynamicLinker(ldconfigPath, rootfsDir);
    auto newEnd = std::remove_if(libs.begin(), libs.end(), isNot64bit);
    libs.erase(newEnd, libs.cend());
    return libs;
}

boost::optional<boost::filesystem::path> GlibcHook::findLibc(const std::vector<boost::filesystem::path>& libs) const {
    auto it = std::find_if(libs.cbegin(), libs.cend(), sarus::common::isLibc);
    if(it == libs.cend()) {
        return {};
    }
    else {
        return *it;
    }
}

bool GlibcHook::containerGlibcHasToBeReplaced(  const boost::filesystem::path& hostLibc,
                                                const boost::filesystem::path& containerLibc) const {
    auto hostSymlinkTarget = sarus::common::realpathWithinRootfs("/", hostLibc);
    auto hostVersion = sarus::common::parseLibcVersion(hostSymlinkTarget);
    auto containerSymlinkTarget = sarus::common::realpathWithinRootfs(rootfsDir, containerLibc);
    auto containerVersion = sarus::common::parseLibcVersion(containerSymlinkTarget);

    if(containerVersion < hostVersion) {
        auto message = boost::format("Detected glibc %1%.%2% (< %3%.%4%) in the container. Replacing it with glibc %3%.%4% from the host."
                                     " Please consider upgrading the container image to a distribution with glibc >= %3%.%4%.")
            % std::get<0>(containerVersion) % std::get<1>(containerVersion)
            % std::get<0>(hostVersion) % std::get<1>(hostVersion);
        logMessage(message, sarus::common::LogLevel::GENERAL, std::cerr);
        return true;
    }
    else {
        return false;
    }
}

void GlibcHook::verifyThatHostAndContainerGlibcAreABICompatible(
    const boost::filesystem::path& hostLibc,
    const boost::filesystem::path& containerLibc) const {
    auto hostSoname = sarus::common::getSharedLibSoname(hostLibc, readelfPath);
    auto containerSoname = sarus::common::getSharedLibSoname(rootfsDir / containerLibc, readelfPath);
    if(hostSoname != containerSoname) {
        auto message = boost::format(
            "Failed to inject glibc libraries. Host's glibc is not ABI compatible with container's glibc."
            "Host has %s, but container has %s") % hostSoname % containerSoname;
        SARUS_THROW_ERROR(message.str());
    }
}

void GlibcHook::replaceGlibcLibrariesInContainer() const {
    bool wasInjectionPerformed = false;

    for(const auto& hostLib : hostLibraries) {
        auto soname = sarus::common::getSharedLibSoname(hostLib, readelfPath);
        for(const auto& containerLib : containerLibraries) {
            if(containerLib.filename().string() == soname) {
                wasInjectionPerformed = true;
                sarus::runtime::bindMount(hostLib, rootfsDir / sarus::common::realpathWithinRootfs(rootfsDir, containerLib));
            }
        }
    }

    if(!wasInjectionPerformed) {
        SARUS_THROW_ERROR("Internal error: failed to inject glibc libraries.");
    }
}

void GlibcHook::logMessage( const std::string& message, sarus::common::LogLevel logLevel,
                            std::ostream& out, std::ostream& err) const {
    auto systemName = "glibc-hook";
    sarus::common::Logger::getInstance().log(message, systemName, logLevel, out, err);
}

void GlibcHook::logMessage( const boost::format& message, sarus::common::LogLevel logLevel,
                            std::ostream& out, std::ostream& err) const {
    auto systemName = "glibc-hook";
    sarus::common::Logger::getInstance().log(message.str(), systemName, logLevel, out, err);
}

}}} // namespace
