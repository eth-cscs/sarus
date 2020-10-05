/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/**
 * @brief Source file for user-requested mounts.
 */

#include "Mount.hpp"

#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/fsuid.h>

#include "common/Utility.hpp"
#include "runtime/Utility.hpp"
#include "runtime/mount_utilities.hpp"


namespace sarus {
namespace runtime {

Mount::Mount(   const boost::filesystem::path& source,
                const boost::filesystem::path& destination,
                const size_t mountFlags,
                std::shared_ptr<const common::Config> config)
    : source{source}
    , destination{destination}
    , mountFlags{mountFlags}
    , config_weak{config}
{}

void Mount::performMount() const {
    runtime::utility::logMessage(boost::format("Performing bind mount: source = %s; target = %s; mount flags = %d")
        % source.string() % destination.string() % mountFlags, common::LogLevel::DEBUG);

    auto config = config_weak.lock();
    if(!config) {
        SARUS_THROW_ERROR("Internal error: failed to lock std::weak_ptr");
    }

    // switch to user identity to make sure user has access to the mount source
    common::switchIdentity(config->userIdentity);

    auto rootfsDir = boost::filesystem::path{ config->json["OCIBundleDir"].GetString() }
        / config->json["rootfsFolder"].GetString();
    auto destinationReal = rootfsDir / common::realpathWithinRootfs(rootfsDir, destination);

    try {
        validateMountSource(source);
        validateMountDestination(destinationReal, *config);
    } catch(std::exception& e) {
        auto message = boost::format("Failed to bind mount %s on container's %s: %s")
            % source.string() % destination.string() % e.what();
        runtime::utility::logMessage(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_RETHROW_ERROR(e, message.str(), common::LogLevel::INFO);
    }

    auto realpathOfSource = std::shared_ptr<char>{realpath(source.string().c_str(), NULL), free};
    if (!realpathOfSource) {
        auto message = boost::format("Failed to find real path for user-requested mount source: %s") % source;
        SARUS_THROW_ERROR(message.str());
    }

    if(boost::filesystem::is_directory(realpathOfSource.get())) {
        common::createFoldersIfNecessary(destinationReal, config->userIdentity.uid, config->userIdentity.gid);
    }
    else {
        common::createFileIfNecessary(destinationReal, config->userIdentity.uid, config->userIdentity.gid);
    }

    auto rootIdentity = common::UserIdentity{};
    common::switchIdentity(rootIdentity);

    try {
        // switch to user filesystem identity to make sure we can access paths as root even on root_squashed filesystems
        common::setFilesystemUid(config->userIdentity);

        bindMount(realpathOfSource.get(), destinationReal, mountFlags);

        common::setFilesystemUid(rootIdentity);
    }
    catch (const common::Error& e) {
            common::setFilesystemUid(rootIdentity);
            SARUS_RETHROW_ERROR(e, std::string("Failed to perform custom mount"));
    }

    runtime::utility::logMessage("Successfully performed bind mount", common::LogLevel::DEBUG);
}

} // namespace
} // namespace
