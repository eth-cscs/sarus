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
 *  @brief Tests for user-requested mounts
 */

#include <string>

#include <sys/stat.h>
#include <sys/mount.h>

#include "test_utility/config.hpp"
#include "test_utility/filesystem.hpp"
#include "runtime/Mount.hpp"
#include "common/PathRAII.hpp"
#include "common/Utility.hpp"
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;


TEST_GROUP(MountTestGroup) {
};

#ifdef ASROOT
TEST(MountTestGroup, mount_test) {
#else
IGNORE_TEST(MountTestGroup, mount_test) {
#endif
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    auto bundleDirRAII = common::PathRAII{boost::filesystem::path{config->json["OCIBundleDir"].GetString()}};
    const auto& bundleDir = bundleDirRAII.getPath();
    auto rootfsDir = bundleDir / boost::filesystem::path{config->json["rootfsFolder"].GetString()};
    auto overlayfsLowerDir = bundleDir / "overlay/rootfs-lower"; // hardcoded so in production code being tested
    common::createFoldersIfNecessary(overlayfsLowerDir);

    auto sourceDirRAII = common::PathRAII{boost::filesystem::path{"./user_mounts_source"}};
    const auto& sourceDir = sourceDirRAII.getPath();
    auto destinationDir = boost::filesystem::path{"/user_mounts_destination"};

    auto sourceFile = common::PathRAII{boost::filesystem::path{"./user_mounts_source_file"}};
    auto destinationFile = common::PathRAII{boost::filesystem::path{"/user_mounts_destination_file"}};

    size_t mount_flags = 0;

    // Create files and directories
    common::createFoldersIfNecessary(rootfsDir);
    test_utility::filesystem::create_test_directory_tree(sourceDir.string());
    common::createFileIfNecessary(sourceFile.getPath());
    auto command = "echo \"test data\" >" + sourceFile.getPath().string();
    auto ret = std::system(command.c_str());
    CHECK(WIFEXITED(ret) != 0 && WEXITSTATUS(ret) == 0);

    // test mount of non-existing destination directory
    {
        runtime::Mount{sourceDir, destinationDir, mount_flags, config}.performMount();
        CHECK(test_utility::filesystem::are_directories_equal(sourceDir.string(), (rootfsDir / destinationDir).string(), 1));

        // cleanup
        CHECK(umount((rootfsDir / destinationDir).c_str()) == 0);
        boost::filesystem::remove_all(rootfsDir / destinationDir);
    }
    // test mount of existing destination directory
    {
        common::createFoldersIfNecessary(rootfsDir / destinationDir);
        runtime::Mount{sourceDir, destinationDir.c_str(), mount_flags, config}.performMount();
        CHECK(test_utility::filesystem::are_directories_equal(sourceDir.string(), (rootfsDir / destinationDir).string(), 1));

        // cleanup
        CHECK(umount((rootfsDir / destinationDir).c_str()) == 0);
        boost::filesystem::remove_all(rootfsDir / destinationDir);
    }
    // test mount of individual file
    {
        runtime::Mount{sourceFile.getPath(), destinationFile.getPath(), mount_flags, config}.performMount();
        CHECK(test_utility::filesystem::isSameBindMountedFile(sourceFile.getPath(), rootfsDir / destinationFile.getPath()));

        // cleanup
        CHECK(umount((rootfsDir / destinationFile.getPath()).c_str()) == 0);
    }
    // test ctor with 5 arguments
    {
        runtime::Mount{sourceFile.getPath(), destinationFile.getPath(), mount_flags, rootfsDir, config->userIdentity}.performMount();
        CHECK(test_utility::filesystem::isSameBindMountedFile(sourceFile.getPath(), rootfsDir / destinationFile.getPath()));

        // cleanup
        CHECK(umount((rootfsDir / destinationFile.getPath()).c_str()) == 0);
    }
    // test default move ctor
    {
        auto mountObject = runtime::Mount{sourceFile.getPath(), destinationFile.getPath(), mount_flags, config};
        runtime::Mount{std::move(mountObject)}.performMount();
        CHECK(test_utility::filesystem::isSameBindMountedFile(sourceFile.getPath(), rootfsDir / destinationFile.getPath()));

        // cleanup
        CHECK(umount((rootfsDir / destinationFile.getPath()).c_str()) == 0);
    }
}

SARUS_UNITTEST_MAIN_FUNCTION();
