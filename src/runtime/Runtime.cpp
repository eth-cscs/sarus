/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Runtime.hpp"

#include <type_traits>
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
    , fdHandler{config}
{
    auto status = common::readFile("/proc/self/status");
    config->commandRun.cpuAffinity = common::getCpuAffinity();
}

void Runtime::setupOCIBundle() {
    utility::logMessage("Setting up OCI Bundle", common::LogLevel::INFO);

    setupMountIsolation();
    setupRamFilesystem();
    mountImageIntoRootfs();
    setupDevFilesystem();
    copyEtcFilesIntoRootfs();
    mountInitProgramIntoRootfsIfNecessary();
    performCustomMounts();
    remountRootfsWithNoSuid();
    fdHandler.preservePMIFdIfAny();
    fdHandler.passStdoutAndStderrToHooks();
    fdHandler.applyChangesToFdsAndEnvVariablesAndBundleAnnotations();
    bundleConfig.generateConfigFile();

    utility::logMessage("Successfully set up OCI Bundle", common::LogLevel::INFO);
}

void Runtime::executeContainer() const {
    auto containerID = "container-" + common::generateRandomString(16);
    utility::logMessage("Executing " + containerID, common::LogLevel::INFO);

    // chdir to bundle
    common::changeDirectory(bundleDir);

    // assemble runc args
    auto runcPath = config->json["runcPath"].GetString();
    auto extraFileDescriptors = std::to_string(fdHandler.getExtraFileDescriptors());
    auto args = common::CLIArguments{runcPath, "run",
                                     "--preserve-fds", extraFileDescriptors,
                                     containerID};

    // execute runc
    auto status = common::forkExecWait(args);
    if(status != 0) {
        auto message = boost::format("%s exited with code %d") % args % status;
        utility::logMessage(message, common::LogLevel::INFO);
        exit(status);
    }

    utility::logMessage("Successfully executed " + containerID, common::LogLevel::INFO);
}

void Runtime::setupMountIsolation() const {
    utility::logMessage("Setting up mount isolation", common::LogLevel::INFO);
    if(unshare(CLONE_NEWNS) != 0) {
        SARUS_THROW_ERROR("Failed to unshare the mount namespace");
    }

    // make sure that there are no MS_SHARED mounts,
    // otherwise our changes could propagate outside the container
    if(mount(NULL, "/", NULL, MS_SLAVE|MS_REC, NULL) != 0) {
        SARUS_THROW_ERROR("Failed to remount \"/\" with MS_SLAVE");
    }
    utility::logMessage("Successfully set up mount isolation", common::LogLevel::INFO);
}

void Runtime::setupRamFilesystem() const {
    utility::logMessage("Setting up RAM filesystem", common::LogLevel::INFO);
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

    utility::logMessage("Successfully set up RAM filesystem", common::LogLevel::INFO);
}

void Runtime::mountImageIntoRootfs() const {
    utility::logMessage("Mounting image into bundle's rootfs", common::LogLevel::INFO);

    auto lowerDir = bundleDir / "overlay/rootfs-lower";
    auto upperDir = bundleDir / "overlay/rootfs-upper";
    auto workDir = bundleDir / "overlay/rootfs-work";
    common::createFoldersIfNecessary(rootfsDir);
    common::createFoldersIfNecessary(lowerDir);
    common::createFoldersIfNecessary(upperDir, config->userIdentity.uid, config->userIdentity.gid);
    common::createFoldersIfNecessary(workDir);

    loopMountSquashfs(config->getImageFile(), lowerDir);
    mountOverlayfs(lowerDir, upperDir, workDir, rootfsDir);

    utility::logMessage("Successfully mounted image into bundle's rootfs", common::LogLevel::INFO);
}

void Runtime::setupDevFilesystem() const {
    utility::logMessage("Setting up /dev filesystem", common::LogLevel::INFO);

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

    utility::logMessage("Successfully set up /dev filesystem", common::LogLevel::INFO);
}

void Runtime::copyEtcFilesIntoRootfs() const {
    utility::logMessage("Copying /etc files into rootfs", common::LogLevel::INFO);
    auto prefixDir = boost::filesystem::path{config->json["prefixDir"].GetString()};

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

    utility::logMessage("Successfully copied /etc files into rootfs", common::LogLevel::INFO);
}

void Runtime::mountInitProgramIntoRootfsIfNecessary() const {
    if(config->commandRun.addInitProcess) {
        utility::logMessage("Mounting init program into rootfs", common::LogLevel::INFO);
        auto src = boost::filesystem::path{ config->json["initPath"].GetString() };
        auto dst = rootfsDir / "dev/init";
        common::createFileIfNecessary(dst);
        bindMount(src, dst);
        utility::logMessage("Successfully mounted init program into rootfs", common::LogLevel::INFO);
    }
}

void Runtime::performCustomMounts() const {
    utility::logMessage("Performing custom mounts", common::LogLevel::INFO);
    for(const auto& mount : config->commandRun.mounts) {
        mount->performMount();
    }
    utility::logMessage("Successfully performed custom mounts", common::LogLevel::INFO);
}

void Runtime::remountRootfsWithNoSuid() const {
    utility::logMessage("Remounting rootfs with MS_NOSUID", common::LogLevel::INFO);
    if (mount(rootfsDir.c_str(), rootfsDir.c_str(), "overlay", MS_REMOUNT|MS_NOSUID, NULL) != 0) {
        auto message = boost::format("Failed to remount rootfs %s with MS_NOSUID: %s") % rootfsDir % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    utility::logMessage("Successfully remounted rootfs with MS_NOSUID", common::LogLevel::INFO);
}

} // namespace
} // namespace
