/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
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

Mount::Mount(const boost::filesystem::path& source,
             const boost::filesystem::path& destination,
             const size_t mountFlags,
             const boost::filesystem::path& rootfsDir,
             const common::UserIdentity userIdentity)
    : source{source}
    , destination{destination}
    , mountFlags{mountFlags}
    , rootfsDir{rootfsDir}
    , userIdentity{userIdentity}
{}

Mount::Mount(const boost::filesystem::path& source,
             const boost::filesystem::path& destination,
             const size_t mountFlags,
             std::shared_ptr<const common::Config> config)
    : source{source}
    , destination{destination}
    , mountFlags{mountFlags}
    , userIdentity{config->userIdentity}
{
    rootfsDir = boost::filesystem::path{ config->json["OCIBundleDir"].GetString() }
                / config->json["rootfsFolder"].GetString();
}

void Mount::performMount() const {
    runtime::utility::logMessage(boost::format("Performing bind mount: source = %s; target = %s; mount flags = %d")
        % source.string() % destination.string() % mountFlags, common::LogLevel::DEBUG);

    try {
        validatedBindMount(source, destination, userIdentity, rootfsDir, mountFlags);
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
