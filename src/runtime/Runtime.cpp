/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Runtime.hpp"

#include <type_traits>
#include <cerrno>
#include <cstring>
#include <functional>
#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/prctl.h>


#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "libsarus/Error.hpp"
#include "libsarus/Utility.hpp"
#include "common/ImageReference.hpp"
#include "libsarus/CLIArguments.hpp"
#include "runtime/Utility.hpp"


namespace sarus {
namespace runtime {

Runtime::Runtime(std::shared_ptr<common::Config> config)
    : config{config}
    , bundleDir{ boost::filesystem::path{config->json["OCIBundleDir"].GetString()} }
    , rootfsDir{ bundleDir / boost::filesystem::path{config->json["rootfsFolder"].GetString()} }
    , bundleConfig{config}
    , fdHandler{config}
{
    libsarus::clearEnvironmentVariables();

    auto status = libsarus::readFile("/proc/self/status");
    config->commandRun.cpuAffinity = libsarus::getCpuAffinity();
}

void Runtime::setupOCIBundle() {
    utility::logMessage("Setting up OCI Bundle", libsarus::LogLevel::INFO);

    setupMountIsolation();
    setupRamFilesystem();
    mountImageIntoRootfs();
    setupDevFilesystem();
    copyEtcFilesIntoRootfs();
    mountInitProgramIntoRootfsIfNecessary();
    performCustomMounts();
    performExtraMounts();
    performDeviceMounts();
    remountRootfsWithNoSuid();
    fdHandler.preservePMIFdIfAny();
    fdHandler.applyChangesToFdsAndEnvVariablesAndBundleAnnotations();
    bundleConfig.generateConfigFile();

    utility::logMessage("Successfully set up OCI Bundle", libsarus::LogLevel::INFO);
}

void Runtime::executeContainer() const {
    auto containerID = "container-" + libsarus::generateRandomString(16);
    utility::logMessage("Executing " + containerID, libsarus::LogLevel::INFO);

    // chdir to bundle
    libsarus::changeDirectory(bundleDir);

    // assemble runc args
    auto runcPath = config->json["runcPath"].GetString();
    auto extraFileDescriptors = std::to_string(fdHandler.getExtraFileDescriptors());
    auto args = libsarus::CLIArguments{runcPath, "run",
                                     "--preserve-fds", extraFileDescriptors,
                                     containerID};

    // prepare a pre-exec function for the forked process (i.e. the OCI runtime)
    // to set a parent-death signal, in the attempt to gracefully terminate the container
    // and cleanup should the Sarus process receive a SIGKILL or die unexpectedly in another way.
    auto setParentDeathSignal = [] (pid_t parentPid) {
        if(prctl(PR_SET_PDEATHSIG, SIGHUP) == -1) {
            auto message = boost::format("Failed to set parent death signal in subprocess for OCI runtime");
            SARUS_THROW_ERROR(message.str());
        }
        // check if the parent already exited before the prctl() call
        if (getppid() != parentPid) {
            auto message = boost::format("Sarus main process died immediately after forking subprocess for OCI runtime");
            SARUS_THROW_ERROR(message.str());
        }
    };

    // execute runc
    auto status = libsarus::forkExecWait(args,
                                       std::function<void()>{std::bind(setParentDeathSignal, getpid())},
                                       std::function<void(pid_t)>{utility::setupSignalProxying});
    if(status != 0) {
        auto message = boost::format("%s exited with code %d") % args % status;
        utility::logMessage(message, libsarus::LogLevel::INFO);
        exit(status);
    }

    utility::logMessage("Successfully executed " + containerID, libsarus::LogLevel::INFO);
}

void Runtime::setupMountIsolation() const {
    utility::logMessage("Setting up mount isolation", libsarus::LogLevel::INFO);
    if(unshare(CLONE_NEWNS) != 0) {
        SARUS_THROW_ERROR("Failed to unshare the mount namespace");
    }

    // make sure that there are no MS_SHARED mounts,
    // otherwise our changes could propagate outside the container
    if(mount(NULL, "/", NULL, MS_SLAVE|MS_REC, NULL) != 0) {
        SARUS_THROW_ERROR("Failed to remount \"/\" with MS_SLAVE");
    }
    utility::logMessage("Successfully set up mount isolation", libsarus::LogLevel::INFO);
}

void Runtime::setupRamFilesystem() const {
    utility::logMessage("Setting up RAM filesystem", libsarus::LogLevel::INFO);
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

    utility::logMessage("Successfully set up RAM filesystem", libsarus::LogLevel::INFO);
}

void Runtime::mountImageIntoRootfs() const {
    utility::logMessage("Mounting image into bundle's rootfs", libsarus::LogLevel::INFO);

    auto lowerDir = bundleDir / "overlay/rootfs-lower";
    auto upperDir = bundleDir / "overlay/rootfs-upper";
    auto workDir = bundleDir / "overlay/rootfs-work";
    libsarus::createFoldersIfNecessary(rootfsDir);
    libsarus::createFoldersIfNecessary(lowerDir);
    libsarus::createFoldersIfNecessary(upperDir, config->userIdentity.uid, config->userIdentity.gid);
    libsarus::createFoldersIfNecessary(workDir);

    libsarus::loopMountSquashfs(config->getImageFile(), lowerDir);
    libsarus::mountOverlayfs(lowerDir, upperDir, workDir, rootfsDir);

    utility::logMessage("Successfully mounted image into bundle's rootfs", libsarus::LogLevel::INFO);
}

void Runtime::setupDevFilesystem() const {
    utility::logMessage("Setting up /dev filesystem", libsarus::LogLevel::INFO);

    const char* ramFilesystemType = config->json["ramFilesystemType"].GetString();
    libsarus::createFoldersIfNecessary(rootfsDir / "dev");
    auto flags = MS_NOSUID | MS_STRICTATIME;
    auto* options = "mode=755,size=65536k";
    if(mount(NULL, (rootfsDir / "dev").c_str(), ramFilesystemType, flags, options) != 0) {
        auto message = boost::format("Failed to setup %s filesystem on %s: %s")
            % ramFilesystemType
            % (rootfsDir / "dev")
            % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    utility::logMessage("Successfully set up /dev filesystem", libsarus::LogLevel::INFO);
}

void Runtime::copyEtcFilesIntoRootfs() const {
    utility::logMessage("Copying /etc files into rootfs", libsarus::LogLevel::INFO);
    auto prefixDir = boost::filesystem::path{config->json["prefixDir"].GetString()};

    libsarus::copyFile(   "/etc/hosts",
                        rootfsDir / "etc/hosts",
                        config->userIdentity.uid, config->userIdentity.gid);

    libsarus::copyFile(   "/etc/resolv.conf",
                        rootfsDir / "etc/resolv.conf",
                        config->userIdentity.uid, config->userIdentity.gid);

    libsarus::copyFile(   prefixDir / "etc/container/nsswitch.conf",
                        rootfsDir / "etc/nsswitch.conf",
                        config->userIdentity.uid, config->userIdentity.gid);

    libsarus::copyFile(   prefixDir / "etc/passwd",
                        rootfsDir / "etc/passwd",
                        config->userIdentity.uid, config->userIdentity.gid);

    libsarus::copyFile(   prefixDir / "etc/group",
                        rootfsDir / "etc/group",
                        config->userIdentity.uid, config->userIdentity.gid);

    utility::logMessage("Successfully copied /etc files into rootfs", libsarus::LogLevel::INFO);
}

void Runtime::mountInitProgramIntoRootfsIfNecessary() const {
    if(config->commandRun.addInitProcess) {
        utility::logMessage("Mounting init program into rootfs", libsarus::LogLevel::INFO);
        auto src = boost::filesystem::path{ config->json["initPath"].GetString() };
        auto dst = rootfsDir / "dev/init";
        libsarus::createFileIfNecessary(dst);
        libsarus::bindMount(src, dst);
        utility::logMessage("Successfully mounted init program into rootfs", libsarus::LogLevel::INFO);
    }
}

/**
 * "Custom mounts" are those defined by users through the CLI ("user mounts") and by the system
 * administrator through the configuration file ("site mounts"). They represent a mean of arbitrary
 * container customization.
 */
void Runtime::performCustomMounts() const {
    utility::logMessage("Performing custom mounts", libsarus::LogLevel::INFO);
    for(const auto& mount : config->commandRun.mounts) {
        mount->performMount();
    }
    utility::logMessage("Successfully performed custom mounts", libsarus::LogLevel::INFO);
}

/**
 * "Extra mounts" are feature-dependent mounts which may happen automatically (i.e. without direct control
 * by users or system administrators), but are not part of basic container setup.
 */
void Runtime::performExtraMounts() const {
    utility::logMessage("Performing extra mounts", libsarus::LogLevel::INFO);
    if (const rapidjson::Value* pmixSupport = rapidjson::Pointer("/enablePMIxv3Support").Get(config->json)) {
        if (pmixSupport->GetBool()) {
            for(const auto& mount : utility::generatePMIxMounts(config)) {
                mount->performMount();
            }
        }
    }
    utility::logMessage("Successfully performed custom mounts", libsarus::LogLevel::INFO);
}

/**
 * "Device mounts" are similar to custom mounts in that they are requested by users or system administrators,
 * however they are grouped separately because, in addition to the mount of the device file, they also require
 * to whitelist the device in the devices cgroup.
 * The whitelisting is delegated to the OCI runtime by entering devices in the bundle config (see OCIBundleConfig class).
 * The OCI Runtime spec states that the runtime MAY supply devices on its own, using the method it prefers:
 * https://github.com/opencontainers/runtime-spec/blob/v1.0.2/config-linux.md#devices
 * We bind mount device files here to have more direct control, in a similar fashion to what is done for /dev.
 */
void Runtime::performDeviceMounts() const {
    utility::logMessage("Performing device mounts", libsarus::LogLevel::INFO);
    for(const auto& mount : config->commandRun.deviceMounts) {
        mount->performMount();
    }
    utility::logMessage("Successfully performed device mounts", libsarus::LogLevel::INFO);
}

void Runtime::remountRootfsWithNoSuid() const {
    utility::logMessage("Remounting rootfs with MS_NOSUID", libsarus::LogLevel::INFO);
    if (mount(rootfsDir.c_str(), rootfsDir.c_str(), "overlay", MS_REMOUNT|MS_NOSUID, NULL) != 0) {
        auto message = boost::format("Failed to remount rootfs %s with MS_NOSUID: %s") % rootfsDir % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    utility::logMessage("Successfully remounted rootfs with MS_NOSUID", libsarus::LogLevel::INFO);
}

} // namespace
} // namespace
