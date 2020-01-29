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
 *  @brief Tests for mount utilities.
 */

#include <string>

#include <sys/mount.h>

#include "test_utility/config.hpp"
#include "test_utility/filesystem.hpp"
#include "common/PathRAII.hpp"
#include "common/Utility.hpp"
#include "runtime/mount_utilities.hpp"
#include "test_utility/unittest_main_function.hpp"


using namespace sarus;

TEST_GROUP(MountUtilitiesTestGroup) {
};

TEST(MountUtilitiesTestGroup, validate_mount_source_test) {
    std::string mount_point("./MUMountPoint");
    std::string source_dir_1("./mount_utilities_source_1");
    common::PathRAII source_dir_2RAII("./mount_utilities_source_2");
    std::string source_dir_2 = source_dir_2RAII.getPath().string();

    // Test invalid input arguments
    CHECK_THROWS(common::Error, runtime::validateMountSource(""));

    // Test non-existing directory
    CHECK_THROWS(common::Error, runtime::validateMountSource(source_dir_1));

    // Test existing directory
    common::executeCommand("mkdir -p " + source_dir_2);
    auto* expected = realpath(source_dir_2.c_str(), NULL);
    runtime::validateMountSource(source_dir_2);

    // Cleanup
    free(expected);
    common::executeCommand("rm -rf " + source_dir_2);
}

TEST(MountUtilitiesTestGroup, validate_mount_destination_test) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = *configRAII.config;
    auto bundleDirRAII = common::PathRAII{boost::filesystem::path{config.json["OCIBundleDir"].GetString()}};
    const auto& bundleDir = bundleDirRAII.getPath();
    auto rootfsDir = bundleDir / boost::filesystem::path{config.json["rootfsFolder"].GetString()};
    common::createFoldersIfNecessary(bundleDir / "overlay/rootfs-lower");

    // Test invalid input arguments
    CHECK_THROWS(common::Error, runtime::validateMountDestination("", config));

    // Test mount on other device
    auto otherDeviceDir = boost::filesystem::path{"/otherDevice"};
    common::createFoldersIfNecessary(rootfsDir / otherDeviceDir);
    runtime::loopMountSquashfs("test_image.squashfs", rootfsDir / otherDeviceDir);
    CHECK_THROWS(common::Error, runtime::validateMountDestination(rootfsDir / otherDeviceDir, config));
    CHECK(umount((rootfsDir / otherDeviceDir).c_str()) == 0);

    // Test non-existing mount point
    auto nonExistingDir = boost::filesystem::path{"/nonExistingMountPoint"};
    runtime::validateMountDestination(rootfsDir / nonExistingDir, config);

    // Test existing mount point
    auto existingDir = boost::filesystem::path{"/file_in_squashfs_image"};
    common::createFoldersIfNecessary(rootfsDir / existingDir);
    runtime::validateMountDestination(rootfsDir / existingDir, config);
}

TEST(MountUtilitiesTestGroup, bindMount) {
    auto tempDirRAII = common::PathRAII{common::makeUniquePathWithRandomSuffix("/tmp/sarus-test-runtime-bindmount")};
    const auto& tempDir = tempDirRAII.getPath();
    auto fromDir = tempDir / "from";
    auto toDir = tempDir / "to";

    common::createFoldersIfNecessary(fromDir);
    common::createFoldersIfNecessary(toDir);
    common::createFileIfNecessary(fromDir / "file");

    runtime::bindMount(fromDir, toDir);

    // check that "file" is in the mounted directory
    CHECK(boost::filesystem::exists(toDir / "file"));

    // check that mounted directory is writable
    common::createFileIfNecessary(toDir / "file-successfull-write-attempt");

    // check that mounted directory is read-only
    CHECK(umount(toDir.c_str()) == 0);
    runtime::bindMount(fromDir, toDir, MS_RDONLY);
    CHECK_THROWS(common::Error, common::createFileIfNecessary(toDir / "file-failed-write-attempt"));

    // cleanup
    CHECK(umount(toDir.c_str()) == 0);
}

TEST(MountUtilitiesTestGroup, loopMountSquashfs) {
    auto mountPointRAII = common::PathRAII{common::makeUniquePathWithRandomSuffix("/tmp/sarus-test-runtime-loopMountSquashfs")};
    const auto& mountPoint = mountPointRAII.getPath();
    common::createFoldersIfNecessary(mountPoint);

    auto imageSquashfs = boost::filesystem::path{__FILE__}.parent_path() / "test_image.squashfs";
    runtime::loopMountSquashfs(imageSquashfs, mountPoint);
    CHECK(boost::filesystem::exists(mountPoint / "file_in_squashfs_image"));

    CHECK(umount(mountPoint.string().c_str()) == 0);
}

SARUS_UNITTEST_MAIN_FUNCTION();
