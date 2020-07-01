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
    // check that directory exists, i.e. is visible to user
    if (!boost::filesystem::exists(source)) {
        SARUS_THROW_ERROR("mount source doesn't exist");
    }
}


void validateMountDestination(const boost::filesystem::path& destination, const common::Config& config) {
    /* If the destination does not exist, check its parents */
    if (!boost::filesystem::exists(destination)) {
        /* Search the first existing parent folder and check that it is on the device
           where we are authorized to create stuff */
        boost::optional<boost::filesystem::path> deepestExistingFolder;
        auto current = boost::filesystem::path("");
        for(const auto& component : destination) {
            current /= component;
            if(boost::filesystem::exists(current)) {
                deepestExistingFolder = current;
            }
            else {
                break;
            }
        }

        if(!deepestExistingFolder) {
            auto message = boost::format("internal error: failed to find existing parent folder of %s") % destination;
            SARUS_THROW_ERROR(message.str());
        }

        if(!isPathOnBindMountableDevice(*deepestExistingFolder, config)) {
            SARUS_THROW_ERROR("mount destination is not on a device allowed for mounts");
        }
    }
    /* If destination exists, check it is on an allowed device */
    else if(!isPathOnBindMountableDevice(destination, config)) {
        SARUS_THROW_ERROR("mount destination is not on a device allowed for mounts");
    }
}


bool isPathOnBindMountableDevice(const boost::filesystem::path& path, const common::Config& config) {
    auto bundleDir = boost::filesystem::path(config.json["OCIBundleDir"].GetString());
    auto rootfsDir = bundleDir / config.json["rootfsFolder"].GetString();

    auto pathDevice = getDevice(path);
    utility::logMessage(boost::format("Target device: %d") % pathDevice, common::LogLevel::DEBUG);

    auto allowedDevices = std::vector<dev_t>{};
    allowedDevices.reserve(4);
    allowedDevices.push_back(getDevice("/tmp"));
    allowedDevices.push_back(getDevice(rootfsDir));
    if(boost::filesystem::exists(rootfsDir / "dev")) {
        allowedDevices.push_back(getDevice(rootfsDir / "dev"));
    }
    auto lowerLayer = bundleDir / "overlay/rootfs-lower";
    allowedDevices.push_back(getDevice(lowerLayer));
    for(const auto& dev : allowedDevices) {
        utility::logMessage(boost::format("Allowed device: %d") % dev, common::LogLevel::DEBUG);
    }

    bool isPathOnAllowedDevice = std::find( allowedDevices.cbegin(),
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
    if(mount("overlay", mountPoint.c_str(), "overlay", MS_MGC_VAL, options.str().c_str()) != 0) {
        auto message = boost::format("Failed to mount OverlayFS on %s (options: %s): %s")
            % mountPoint % options % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}

} // namespace
} // namespace
