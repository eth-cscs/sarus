/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Runtime.hpp"

#include <cerrno>
#include <cstring>
#include <sched.h>
#include <sys/types.h>
#include <sys/mount.h>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/Utility.hpp"
#include "common/ImageID.hpp"
#include "common/CLIArguments.hpp"
#include "runtime/Utility.hpp"
#include "runtime/mount_utilities.hpp"


namespace sarus {
namespace runtime {

Runtime::Runtime(std::shared_ptr<common::Config> config)
    : config{config}
    , bundleDir{ boost::filesystem::path{config->json["OCIBundleDir"].GetString()} }
    , rootfsDir{ bundleDir / boost::filesystem::path{config->json["rootfsFolder"].GetString()} }
    , bundleConfig{config}
    , descriptorHandler{config}
    , securityChecks{config}
{}

void Runtime::setupOCIBundle() {
    utility::logMessage("Setting up OCI Bundle", common::logType::INFO);
    setupMountIsolation();
    setupRamFilesystem();
    mountImageIntoRootfs();
    setupDevFilesystem();
    copyEtcFilesIntoRootfs();
    performCustomMounts();
    remountRootfsWithNoSuid();
    descriptorHandler.preservePMIFdIfAny();
    descriptorHandler.applyChangesToFdsAndEnvVariables();
    bundleConfig.generateConfigFile();
    securityChecks.checkThatPathIsUntamperable(bundleConfig.getConfigFile());
    securityChecks.checkThatOCIHooksAreUntamperable();
    utility::logMessage("Successfully set up OCI Bundle", common::logType::INFO);
}

void Runtime::executeContainer() const {
    auto containerID = "container-" + common::generateRandomString(16);
    utility::logMessage("Executing " + containerID, common::logType::INFO);

    // chdir to bundle
    common::changeDirectory(bundleDir);

    // execute runc
    auto runcPath = config->json["runcPath"].GetString();
    auto extraFileDescriptors = std::to_string(descriptorHandler.getExtraFileDescriptors());
    auto args = common::CLIArguments{runcPath, "run", "--no-pivot",
                                     "--preserve-fds", extraFileDescriptors,
                                     containerID};
    common::forkExecWait(args);

    utility::logMessage("Successfully executed " + containerID, common::logType::INFO);
}

void Runtime::setupMountIsolation() const {
    utility::logMessage("Setting up mount isolation", common::logType::INFO);
    if(unshare(CLONE_NEWNS) != 0) {
        SARUS_THROW_ERROR("Failed to unshare the mount namespace");
    }

    // make sure that there are no MS_SHARED mounts,
    // otherwise our changes could propagate outside the container
    if(mount(NULL, "/", NULL, MS_SLAVE|MS_REC, NULL) != 0) {
        SARUS_THROW_ERROR("Failed to remount \"/\" with MS_SLAVE");
    }
    utility::logMessage("Successfully set up mount isolation", common::logType::INFO);
}

void Runtime::setupRamFilesystem() const {
    utility::logMessage("Setting up RAM filesystem", common::logType::INFO);
    const char* ramFilesystemType = config->json["ramFilesystemType"].GetString();

    if(mount(NULL, bundleDir.c_str(), ramFilesystemType, MS_NOSUID|MS_NODEV, NULL) != 0) {
        auto message = boost::format("Failed to setup %s filesystem on %s: %s")
            % ramFilesystemType
            % bundleDir
            % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    // make sure that mount is MS_SLAVE (could not be the default)
    if(mount(NULL, bundleDir.c_str(), NULL, MS_SLAVE|MS_REC, NULL) != 0) {
        auto message = boost::format("Failed to remount %s with MS_SLAVE") % bundleDir;
        SARUS_THROW_ERROR(message.str());
    }

    // ensure permissions to the bundle directory comply with security checks
    // the permission change could be embedded in the mount command using a tmpfs-specific mode option,
    // but an explicit permission change works also for ramfs
    auto bundleDirPermissions = boost::filesystem::owner_all   |
                                boost::filesystem::group_read  | boost::filesystem::group_exe |
                                boost::filesystem::others_read | boost::filesystem::others_exe;
    boost::filesystem::permissions(bundleDir, bundleDirPermissions);

    utility::logMessage("Successfully set up RAM filesystem", common::logType::INFO);
}

void Runtime::mountImageIntoRootfs() const {
    utility::logMessage("Mounting image into bundle's rootfs", common::logType::INFO);

    auto lowerDir = bundleDir / "overlay/rootfs-lower";
    auto upperDir = bundleDir / "overlay/rootfs-upper";
    auto workDir = bundleDir / "overlay/rootfs-work";
    common::createFoldersIfNecessary(rootfsDir);
    common::createFoldersIfNecessary(lowerDir);
    common::createFoldersIfNecessary(upperDir, config->userIdentity.uid, config->userIdentity.gid);
    common::createFoldersIfNecessary(workDir);

    loopMountSquashfs(config->getImageFile(), lowerDir);
    mountOverlayfs(lowerDir, upperDir, workDir, rootfsDir);

    utility::logMessage("Successfully mounted image into bundle's rootfs", common::logType::INFO);
}

void Runtime::setupDevFilesystem() const {
    utility::logMessage("Setting up /dev filesystem", common::logType::INFO);

    const char* ramFilesystemType = config->json["ramFilesystemType"].GetString();
    common::createFoldersIfNecessary(rootfsDir / "dev");
    auto flags = MS_NOSUID | MS_STRICTATIME;
    auto* options = "mode=755,size=65536k";
    if(mount(NULL, (rootfsDir / "dev").c_str(), ramFilesystemType, flags, options) != 0) {
        auto message = boost::format("Failed to setup %s filesystem on %s: %s")
            % ramFilesystemType
            % (rootfsDir / "dev")
            % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    utility::logMessage("Successfully set up /dev filesystem", common::logType::INFO);
}

void Runtime::copyEtcFilesIntoRootfs() const {
    utility::logMessage("Copying /etc files into rootfs", common::logType::INFO);
    auto prefixDir = boost::filesystem::path{config->json["prefixDir"].GetString()};
    common::createFoldersIfNecessary(rootfsDir / "etc", config->userIdentity.uid, config->userIdentity.gid);

    common::copyFile(   "/etc/hosts",
                        rootfsDir / "etc/hosts",
                        config->userIdentity.uid, config->userIdentity.gid);

    common::copyFile(   "/etc/resolv.conf",
                        rootfsDir / "etc/resolv.conf",
                        config->userIdentity.uid, config->userIdentity.gid);

    common::copyFile(   prefixDir / "etc/container/nsswitch.conf",
                        rootfsDir / "etc/nsswitch.conf",
                        config->userIdentity.uid, config->userIdentity.gid);

    common::copyFile(   prefixDir / "etc/passwd",
                        rootfsDir / "etc/passwd",
                        config->userIdentity.uid, config->userIdentity.gid);

    common::copyFile(   prefixDir / "etc/group",
                        rootfsDir / "etc/group",
                        config->userIdentity.uid, config->userIdentity.gid);

    utility::logMessage("Successfully copied /etc files into rootfs", common::logType::INFO);
}


void Runtime::performCustomMounts() const {
    utility::logMessage("Performing custom mounts", common::logType::INFO);
    for(const auto& mount : config->commandRun.mounts) {
        mount->performMount();
    }
    utility::logMessage("Successfully performed custom mounts", common::logType::INFO);
}

void Runtime::remountRootfsWithNoSuid() const {
    utility::logMessage("Remounting rootfs with MS_NOSUID", common::logType::INFO);
    if (mount(rootfsDir.c_str(), rootfsDir.c_str(), "overlay", MS_REMOUNT|MS_NOSUID, NULL) != 0) {
        auto message = boost::format("Failed to remount rootfs %s with MS_NOSUID: %s") % rootfsDir % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    utility::logMessage("Successfully remounted rootfs with MS_NOSUID", common::logType::INFO);
}

} // namespace
} // namespace
