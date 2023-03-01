/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
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

TEST(MountUtilitiesTestGroup, get_validated_mount_source_test) {
    std::string mount_point("./MUMountPoint");
    std::string source_dir_1("./mount_utilities_source_1");
    common::PathRAII source_dir_2RAII("./mount_utilities_source_2");
    std::string source_dir_2 = source_dir_2RAII.getPath().string();

    // Test invalid input arguments
    CHECK_THROWS(common::Error, runtime::getValidatedMountSource(""));

    // Test non-existing directory
    CHECK_THROWS(common::Error, runtime::getValidatedMountSource(source_dir_1));

    // Test existing directory
    common::createFoldersIfNecessary(source_dir_2);
    auto* expected = realpath(source_dir_2.c_str(), NULL);
    CHECK(runtime::getValidatedMountSource(source_dir_2) == boost::filesystem::path(expected));

    // Cleanup
    free(expected);
    boost::filesystem::remove_all(source_dir_2);
}

TEST(MountUtilitiesTestGroup, get_validated_mount_destination_test) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = *configRAII.config;
    auto bundleDirRAII = common::PathRAII{boost::filesystem::path{config.json["OCIBundleDir"].GetString()}};
    const auto& bundleDir = bundleDirRAII.getPath();
    auto rootfsDir = bundleDir / boost::filesystem::path{config.json["rootfsFolder"].GetString()};
    common::createFoldersIfNecessary(bundleDir / "overlay/rootfs-lower");

    // Test invalid input arguments
    CHECK_THROWS(common::Error, runtime::getValidatedMountDestination("", rootfsDir));

    // Test mount on other device
    auto otherDeviceDir = boost::filesystem::path{"/otherDevice"};
    common::createFoldersIfNecessary(rootfsDir / otherDeviceDir);
    auto imageSquashfs = boost::filesystem::path{__FILE__}.parent_path() / "test_image.squashfs";
    runtime::loopMountSquashfs(imageSquashfs, rootfsDir / otherDeviceDir);
    CHECK_THROWS(common::Error, runtime::getValidatedMountDestination(otherDeviceDir, rootfsDir));
    CHECK(umount((rootfsDir / otherDeviceDir).c_str()) == 0);

    // Test non-existing mount point
    auto nonExistingDir = boost::filesystem::path{"/nonExistingMountPoint"};
    auto expected = rootfsDir / nonExistingDir;
    CHECK(runtime::getValidatedMountDestination(nonExistingDir, rootfsDir) == expected);

    // Test existing mount point
    auto existingDir = boost::filesystem::path{"/file_in_squashfs_image"};
    expected = rootfsDir / existingDir;
    common::createFoldersIfNecessary(expected);
    CHECK(runtime::getValidatedMountDestination(existingDir, rootfsDir) == expected);
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

    // cleanup
    CHECK(umount(toDir.c_str()) == 0);
}

TEST(MountUtilitiesTestGroup, bindMountReadOnly) {
    auto tempDirRAII = common::PathRAII{common::makeUniquePathWithRandomSuffix("/tmp/sarus-test-runtime-bindmount")};
    const auto& tempDir = tempDirRAII.getPath();
    auto fromDir = tempDir / "from";
    auto toDir = tempDir / "to";

    common::createFoldersIfNecessary(fromDir);
    common::createFoldersIfNecessary(toDir);
    common::createFileIfNecessary(fromDir / "file");

    runtime::bindMount(fromDir, toDir, MS_RDONLY);

    // check that "file" is in the mounted directory
    CHECK(boost::filesystem::exists(toDir / "file"));

    // check that mounted directory is read-only
    CHECK_THROWS(common::Error, common::createFileIfNecessary(toDir / "file-failed-write-attempt"));

    // cleanup
    CHECK(umount(toDir.c_str()) == 0);
}

TEST(MountUtilitiesTestGroup, bindMountRecursive) {
    auto tempDirRAII = common::PathRAII{common::makeUniquePathWithRandomSuffix("/tmp/sarus-test-runtime-bindmount")};
    const auto& tempDir = tempDirRAII.getPath();

    auto a = tempDir / "a";
    auto b = tempDir / "b";
    auto c = tempDir / "c";
    common::createFoldersIfNecessary(a);
    common::createFoldersIfNecessary(b);
    common::createFoldersIfNecessary(c);

    common::createFileIfNecessary(c / "d.txt");

    // check that "d.txt" is in the mounted directory
    CHECK(!boost::filesystem::exists(b / "d.txt"));
    runtime::bindMount(c, b);
    CHECK(boost::filesystem::exists(b / "d.txt"));

    // check that mounts are recursive by default
    CHECK(!boost::filesystem::exists(a / "d.txt"));
    runtime::bindMount(b, a);
    CHECK(boost::filesystem::exists(a / "d.txt"));

    // cleanup
    CHECK(umount(b.c_str()) == 0);
    CHECK(umount(a.c_str()) == 0);
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
