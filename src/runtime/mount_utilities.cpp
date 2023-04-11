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
 *  @brief General utility functions for custom mounts
 */

#include "mount_utilities.hpp"

#include <errno.h>

#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "common/Utility.hpp"
#include "common/CLIArguments.hpp"
#include "runtime/Utility.hpp"


namespace sarus {
namespace runtime {

boost::filesystem::path getValidatedMountSource(const boost::filesystem::path& source) {
    utility::logMessage(boost::format("Validating mount source: %s") % source, common::LogLevel::DEBUG);
    auto* realPtr = realpath(source.c_str(), NULL);
    if (realPtr == nullptr) {
        auto message = boost::format("Failed to find real path for mount source: %s") % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    auto sourceReal = boost::filesystem::path(realPtr);
    free(realPtr);

    if (!boost::filesystem::exists(sourceReal)) {
        auto message = boost::format("Real path of mount source does not exist: %s") % sourceReal;
        SARUS_THROW_ERROR(message.str());
    }
    utility::logMessage(boost::format("Returning successfully validated mount source: %s") % sourceReal,
                        common::LogLevel::DEBUG);
    return sourceReal;
}


boost::filesystem::path getValidatedMountDestination(const boost::filesystem::path& destination,
                                                     const boost::filesystem::path& rootfsDir) {
    utility::logMessage(boost::format("Validating mount destination: %s") % destination, common::LogLevel::DEBUG);

    if (destination.is_relative()) {
        SARUS_THROW_ERROR("Internal error: destination is not an absolute path");
    }
    if (rootfsDir.is_relative()) {
        SARUS_THROW_ERROR("Internal error: rootfsDir is not an absolute path");
    }
    auto destinationReal = rootfsDir / common::realpathWithinRootfs(rootfsDir, destination);

    /* If the destination does not exist, check its parents */
    if (!boost::filesystem::exists(destinationReal)) {
        /* Search the first existing parent folder and check that it is on the device
           where we are authorized to create stuff */
        boost::optional<boost::filesystem::path> deepestExistingFolder;
        auto current = boost::filesystem::path("");
        for (const auto& component : destinationReal) {
            current /= component;
            if (boost::filesystem::exists(current)) {
                deepestExistingFolder = current;
            }
            else {
                break;
            }
        }

        if (!deepestExistingFolder) {
            auto message = boost::format("Internal error: failed to find existing parent folder of %s") % destination;
            SARUS_THROW_ERROR(message.str());
        }
        utility::logMessage(boost::format("Deepest path for such path is %s") % *deepestExistingFolder, common::LogLevel::DEBUG);

        if (!isPathOnAllowedDevice(*deepestExistingFolder, rootfsDir)) {
            auto message = boost::format("Mount destination (%s) is not on a device allowed for mounts") % (*deepestExistingFolder);
            SARUS_THROW_ERROR(message.str());
        }
    }
    /* If destination exists, check it is on an allowed device */
    else {
        bool allowed;
        if (boost::filesystem::is_directory(destinationReal)) {
            allowed = isPathOnAllowedDevice(destinationReal, rootfsDir);
        }
        else {
            allowed = isPathOnAllowedDevice(destinationReal.parent_path(), rootfsDir);
        }
        if (!allowed) {
            auto message = boost::format("Mount destination (%s) is not on a device allowed for mounts") % destination;
            SARUS_THROW_ERROR(message.str());
        }
    }
    utility::logMessage(boost::format("Returning successfully validated mount destination: %s") % destinationReal,
                        common::LogLevel::DEBUG);
    return destinationReal;
}


bool isPathOnAllowedDevice(const boost::filesystem::path& path, const boost::filesystem::path& rootfsDir) {
    auto pathDevice = getDevice(path);
    utility::logMessage(boost::format("Target device for path %s is: %d") % path % pathDevice, common::LogLevel::DEBUG);

    auto allowedDevices = std::vector<dev_t>{};
    allowedDevices.reserve(4);
    utility::logMessage("Allowed devices are:", common::LogLevel::DEBUG);

    auto dev = getDevice("/tmp");
    allowedDevices.push_back(dev);
    utility::logMessage(boost::format("%d: /tmp") % dev, common::LogLevel::DEBUG);

    dev = getDevice(rootfsDir);
    allowedDevices.push_back(dev);
    utility::logMessage(boost::format("%d: rootfsDir (%s)") % dev % rootfsDir, common::LogLevel::DEBUG);

    if (boost::filesystem::exists(rootfsDir / "dev")) {
        dev = getDevice(rootfsDir / "dev");
        allowedDevices.push_back(dev);
        utility::logMessage(boost::format("%d: %s/dev") % dev % rootfsDir, common::LogLevel::DEBUG);
    }

    auto bundleDir = rootfsDir.parent_path();
    auto lowerLayer = bundleDir / "overlay/rootfs-lower";
    if (boost::filesystem::exists(lowerLayer)) {
        // rootfs-lower will only be available during container preparation before overlay mount
        // but this method could be used from within the container
        dev = getDevice(lowerLayer);
        allowedDevices.push_back(dev);
        utility::logMessage(boost::format("%d: rootfs-lower (%s)") % dev % lowerLayer, common::LogLevel::DEBUG);
    }

    bool isPathOnAllowedDevice = std::find(allowedDevices.cbegin(),
                                           allowedDevices.cend(),
                                           pathDevice)
                                != allowedDevices.cend();
    return isPathOnAllowedDevice;
}


dev_t getDevice(const boost::filesystem::path& path) {
    struct stat sb;
    if(stat(path.c_str(), &sb) != 0) {
        auto message = boost::format("Failed to stat %s: %s") % path % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    return sb.st_dev;
}

/**
 * Performs a bind mount after validating that source and destination paths are suitable for use.
 * Important things to note:
 * - the source argument does not need to be realpath'ed;
 * - the destination argument needs to be from a point of view **within the container**, and also
 *   does not need to be realpath'ed beforehand.
 * In both cases, this function takes care of resolving and constructing the full paths on its own.
 */
void validatedBindMount(const boost::filesystem::path& source,
                        const boost::filesystem::path& destination,
                        const sarus::common::UserIdentity& userIdentity,
                        const boost::filesystem::path& rootfsDir,
                        const unsigned long flags) {
    auto rootIdentity = sarus::common::UserIdentity{};

    try {
        // switch to user identity to make sure user has access to the mount source
        sarus::common::switchIdentity(userIdentity);
        auto sourceReal = getValidatedMountSource(source);
        auto destinationReal = getValidatedMountDestination(destination, rootfsDir);

        // Save predicate result in a variable. This is done before switching back to root identity to leverage
        // the unprivileged user identity on root_squashed filesystems. The creation of the mount point later
        // on has to be done as root to enable mounts to the root-owned /dev directory in the container.
        // Using the Boost filesystem predicate function as root will be denied if the mount source is in a
        // root_squashed filesystem.
        auto mountSourceIsDirectory = boost::filesystem::is_directory(sourceReal);
        sarus::common::switchIdentity(rootIdentity);

        // Create file or folder if necessary, after validation.
        // Always assign ownership of the newly-created mount point to the container user:
        // while it has no effect on the ownership and permissions of the mounted resource in the container
        // (they are the same as the mount source), a non-root-owned file reduces cleanup problems (in case there are any).
        if (mountSourceIsDirectory){
            sarus::common::createFoldersIfNecessary(destinationReal, userIdentity.uid, userIdentity.gid);
        }
        else {
            sarus::common::createFileIfNecessary(destinationReal, userIdentity.uid, userIdentity.gid);
        }

        // switch to user filesystem identity to make sure we can access paths as root even on root_squashed filesystems
        common::setFilesystemUid(userIdentity);
        bindMount(sourceReal, destinationReal, flags);
        common::setFilesystemUid(rootIdentity);
    }
    catch(sarus::common::Error& e) {
        // Restore root identity in case the exception happened while having a non-privileged id.
        // By setting the euid, the fsuid will also be set accordingly.
        sarus::common::switchIdentity(rootIdentity);
        auto message = boost::format("Failed to bind mount %s on container's %s: %s")
                       % source.string() % destination.string() % e.what();
        SARUS_RETHROW_ERROR(e, message.str());
    }
}


void bindMount(const boost::filesystem::path& from, const boost::filesystem::path& to, unsigned long flags) {
    utility::logMessage(boost::format{"Bind mounting %s -> %s"} % from % to, common::LogLevel::DEBUG);

    unsigned long flagsForBindMount = MS_BIND | MS_REC;
    unsigned long flagsForRemount = MS_REMOUNT | MS_BIND | MS_NOSUID | MS_REC;
    unsigned long flagsForPropagationRemount = MS_PRIVATE | MS_REC;

    if(flags & MS_RDONLY) {
        flagsForRemount |= MS_RDONLY;
    }

    // bind mount
    if(mount(from.c_str(), to.c_str(), "bind", flagsForBindMount, NULL) != 0) {
        auto message = boost::format("Failed to bind mount %s -> %s (error: %s)") % from % to % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    // remount to apply flags
    if(mount(from.c_str(), to.c_str(), "bind", flagsForRemount, NULL) != 0) {
        auto message = boost::format("Failed to re-bind mount %s -> %s (error: %s)") % from % to % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    // remount to apply propagation type
    if(mount(NULL, to.c_str(), NULL, flagsForPropagationRemount, NULL) != 0) {
        auto message = boost::format("Failed to remount %s as non-shared (error: %s)") % to % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}


void loopMountSquashfs(const boost::filesystem::path& image, const boost::filesystem::path& mountPoint) {
    auto command = std::string{"mount"};
    command += " -n";
    command += " -o";
    command += " loop,nosuid,nodev,ro";
    command += " -t squashfs";
    command += " " + image.string();
    command += " " + mountPoint.string();

    utility::logMessage(boost::format{"Performing loop mount: %s "} % command, common::LogLevel::DEBUG);

    try {
        common::executeCommand(command);
    }
    catch(common::Error& e) {
        auto message = boost::format("Failed to loop mount %s on %s") % image % mountPoint;
        SARUS_RETHROW_ERROR(e, message.str());
    }
}


void mountOverlayfs(const boost::filesystem::path& lowerDir,
                    const boost::filesystem::path& upperDir,
                    const boost::filesystem::path& workDir,
                    const boost::filesystem::path& mountPoint) {
    auto options = boost::format{"lowerdir=%s,upperdir=%s,workdir=%s"}
        % lowerDir.string()
        % upperDir.string()
        % workDir.string();
    utility::logMessage(boost::format{"Performing overlay mount to %s "} % mountPoint, common::LogLevel::DEBUG);
    utility::logMessage(boost::format{"Overlay options: %s "} % options.str(), common::LogLevel::DEBUG);
    if(mount("overlay", mountPoint.c_str(), "overlay", MS_MGC_VAL, options.str().c_str()) != 0) {
        auto message = boost::format("Failed to mount OverlayFS on %s (options: %s): %s")
            % mountPoint % options % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}

} // namespace
} // namespace
