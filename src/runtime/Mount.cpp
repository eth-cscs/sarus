/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
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

    auto rootfsDir = boost::filesystem::path{ config->json["OCIBundleDir"].GetString() }
                     / config->json["rootfsFolder"].GetString();
    try {
        validatedBindMount(source, destination, config->userIdentity, rootfsDir, mountFlags);
    }
    catch (const common::Error& e) {
        runtime::utility::logMessage(e.getErrorTrace().back().errorMessage.c_str(),
                                     common::LogLevel::GENERAL, std::cerr);
        SARUS_RETHROW_ERROR(e, std::string("Failed to perform custom bind mount"), common::LogLevel::INFO);
    }

    runtime::utility::logMessage("Successfully performed bind mount", common::LogLevel::DEBUG);
}

} // namespace
} // namespace
