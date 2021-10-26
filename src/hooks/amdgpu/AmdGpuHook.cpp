/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "AmdGpuHook.hpp"

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
namespace amdgpu {

AmdGpuHook::AmdGpuHook() {
    log("Initializing hook", sarus::common::LogLevel::INFO);

    std::tie(bundleDir, pidOfContainer) = hooks::common::utility::parseStateOfContainerFromStdin();
    sarus::hooks::common::utility::enterNamespacesOfProcess(pidOfContainer);
    parseConfigJSONOfBundle();

    log("Successfully initialized hook", sarus::common::LogLevel::INFO);
}

void AmdGpuHook::activate() {
    log("Activating AMD GPU support", sarus::common::LogLevel::INFO);

    bindMountDevices();

    log("Successfully activated AMD GPU support", sarus::common::LogLevel::INFO);
}

void AmdGpuHook::parseConfigJSONOfBundle() {
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

    // get uid + gid of user
    uidOfUser = json["process"]["user"]["uid"].GetInt();
    gidOfUser = json["process"]["user"]["gid"].GetInt();

    log("Successfully parsed bundle's config.json", sarus::common::LogLevel::INFO);
}

void AmdGpuHook::bindMountDevices() const {
    log("Performing bind mounts", sarus::common::LogLevel::INFO);

    for(const auto& mount : {"/dev/kfd", "/dev/dri"}) {
        validatedBindMount(mount, rootfsDir / mount, MS_REC);
    }

    log("Successfully performed bind mounts", sarus::common::LogLevel::INFO);
}


void AmdGpuHook::validatedBindMount(const boost::filesystem::path& from, const boost::filesystem::path& to, unsigned long flags) const {
    auto rootIdentity = sarus::common::UserIdentity{};
    auto userIdentity = sarus::common::UserIdentity(uidOfUser, gidOfUser, {});

    // Validate mount source is visible for user and destination is on allowed device
    sarus::common::switchIdentity(userIdentity);
    sarus::runtime::validateMountSource(from);
    sarus::runtime::validateMountDestination(to, bundleDir, rootfsDir);
    sarus::common::switchIdentity(rootIdentity);

    // Create file or folder if necessary, after validation
    if (boost::filesystem::is_directory(from)){
        sarus::common::createFoldersIfNecessary(to);
    }
    else {
        sarus::common::createFileIfNecessary(to);
    }
    sarus::runtime::bindMount(from, to, flags);
}

void AmdGpuHook::log(const std::string& message, sarus::common::LogLevel level) const {
    sarus::common::Logger::getInstance().log(message, "AMD GPU hook", level);
}

void AmdGpuHook::log(const boost::format& message, sarus::common::LogLevel level) const {
    sarus::common::Logger::getInstance().log(message.str(), "AMD GPU hook", level);
}

}}} // namespace
