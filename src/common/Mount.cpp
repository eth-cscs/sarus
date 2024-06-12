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


namespace sarus {
namespace common {

Mount::Mount(const boost::filesystem::path& source,
             const boost::filesystem::path& destination,
             const size_t mountFlags,
             const boost::filesystem::path& rootfsDir,
             const UserIdentity userIdentity)
    : source{source}
    , destination{destination}
    , mountFlags{mountFlags}
    , rootfsDir{rootfsDir}
    , userIdentity{userIdentity}
{}

Mount::Mount(const boost::filesystem::path& source,
             const boost::filesystem::path& destination,
             const size_t mountFlags,
             std::shared_ptr<const Config> config)
    : source{source}
    , destination{destination}
    , mountFlags{mountFlags}
    , userIdentity{config->userIdentity}
{
    rootfsDir = boost::filesystem::path{ config->json["OCIBundleDir"].GetString() }
                / config->json["rootfsFolder"].GetString();
}

void Mount::performMount() const {
    logMessage(boost::format("Performing bind mount: source = %s; target = %s; mount flags = %d")
        % source.string() % destination.string() % mountFlags, LogLevel::DEBUG);

    try {
        validatedBindMount(source, destination, userIdentity, rootfsDir, mountFlags);
    }
    catch (const Error& e) {
        logMessage(e.getErrorTrace().back().errorMessage.c_str(),
                                     LogLevel::GENERAL, std::cerr);
        SARUS_RETHROW_ERROR(e, std::string("Failed to perform custom bind mount"), LogLevel::INFO);
    }

    logMessage("Successfully performed bind mount", LogLevel::DEBUG);
}

} // namespace
} // namespace
