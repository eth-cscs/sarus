/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_utility_mount_hpp
#define libsarus_utility_mount_hpp

#include <cstddef>
#include <sys/stat.h>
#include <sys/mount.h>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "libsarus/Logger.hpp"
#include "libsarus/Mount.hpp"

/**
 * Utility functions for mounting
 */

namespace libsarus {
namespace mount {

boost::filesystem::path getValidatedMountSource(const boost::filesystem::path&);
boost::filesystem::path getValidatedMountDestination(const boost::filesystem::path& destination,
                                                     const boost::filesystem::path& rootfsDir);
bool isPathOnAllowedDevice(const boost::filesystem::path& path, const boost::filesystem::path& rootfsDir);
dev_t getDevice(const boost::filesystem::path& path);
void validatedBindMount(const boost::filesystem::path& source,
                        const boost::filesystem::path& destination,
                        const UserIdentity& userIdentity,
                        const boost::filesystem::path& rootfsDir,
                        const unsigned long flags=0);
void bindMount(const boost::filesystem::path& from, const boost::filesystem::path& to, unsigned long flags=0);
void loopMountSquashfs(const boost::filesystem::path& image, const boost::filesystem::path& mountPoint);
void mountOverlayfs(const boost::filesystem::path& lowerDir,
                    const boost::filesystem::path& upperDir,
                    const boost::filesystem::path& workDir,
                    const boost::filesystem::path& mountPoint);

}}

#endif
