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

void validateMountSource(const boost::filesystem::path& source) {
    utility::logMessage(boost::format("Validating mount source: %s") % source, common::LogLevel::DEBUG);
    // check that directory exists, i.e. is visible to user
    if (!boost::filesystem::exists(source)) {
        SARUS_THROW_ERROR("mount source doesn't exist");
    }
    utility::logMessage(std::string("Mount source successfully validated"), common::LogLevel::DEBUG);
}


void validateMountDestination(const boost::filesystem::path& destination, const common::Config& config) {
    auto bundleDir = boost::filesystem::path(config.json["OCIBundleDir"].GetString());
    auto rootfsDir = bundleDir / config.json["rootfsFolder"].GetString();
    return validateMountDestination(destination, bundleDir, rootfsDir);
}


void validateMountDestination(const boost::filesystem::path& destination, const boost::filesystem::path& bundleDir, const boost::filesystem::path& rootfsDir) {
    utility::logMessage(boost::format("Validating mount destination: %s") % destination, common::LogLevel::DEBUG);

    /* If the destination does not exist, check its parents */
    if (!boost::filesystem::exists(destination)) {
        /* Search the first existing parent folder and check that it is on the device
           where we are authorized to create stuff */
        boost::optional<boost::filesystem::path> deepestExistingFolder;
        auto current = boost::filesystem::path("");
        for (const auto& component : destination) {
            current /= component;
            if (boost::filesystem::exists(current)) {
                deepestExistingFolder = current;
            }
            else {
                break;
            }
        }

        if (!deepestExistingFolder) {
            auto message = boost::format("internal error: failed to find existing parent folder of %s") % destination;
            SARUS_THROW_ERROR(message.str());
        }
        utility::logMessage(boost::format("Deepest path for such path is %s") % *deepestExistingFolder, common::LogLevel::DEBUG);

        if (!isPathOnAllowedDevice(*deepestExistingFolder, bundleDir, rootfsDir)) {
            auto message = boost::format("mount destination (%s) is not on a device allowed for mounts") % (*deepestExistingFolder);
            SARUS_THROW_ERROR(message.str());
        }
    }
    /* If destination exists, check it is on an allowed device */
    else {
        bool allowed;
        if (boost::filesystem::is_directory(destination)) {
            allowed = isPathOnAllowedDevice(destination, bundleDir, rootfsDir);
        }
        else {
            allowed = isPathOnAllowedDevice(destination.parent_path(), bundleDir, rootfsDir);
        }
        if (!allowed) {
            auto message = boost::format("mount destination (%s) is not on a device allowed for mounts") % destination;
            SARUS_THROW_ERROR(message.str());
        }
    }
    utility::logMessage(std::string("Mount destination successfully validated"), common::LogLevel::DEBUG);
}


bool isPathOnAllowedDevice(const boost::filesystem::path& path, const boost::filesystem::path& bundleDir, const boost::filesystem::path& rootfsDir) {
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


void bindMount(const boost::filesystem::path& from, const boost::filesystem::path& to, unsigned long flags) {
    utility::logMessage(boost::format{"Bind mounting %s -> %s"} % from % to, common::LogLevel::DEBUG);

    unsigned long flagsForBindMount = MS_BIND;
    unsigned long flagsForRemount = MS_REMOUNT | MS_BIND | MS_NOSUID;
    unsigned long flagsForPropagationRemount = 0;

    if(flags & MS_RDONLY) {
        flagsForRemount |= MS_RDONLY;
    }

    // allow only MS_PRIVATE and MS_SLAVE propagation flags and default to MS_PRIVATE
    if(flags & MS_PRIVATE) {
        flagsForPropagationRemount = MS_PRIVATE;
    }
    else if(flags & MS_SLAVE) {
        flagsForPropagationRemount = MS_SLAVE;
    }
    else {
        flagsForPropagationRemount = MS_PRIVATE;
    }

    if (flags & MS_REC) {
        flagsForBindMount |= MS_REC;
        flagsForRemount |= MS_REC;
        flagsForPropagationRemount |= MS_REC;
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
