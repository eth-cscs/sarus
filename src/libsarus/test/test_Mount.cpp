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
 *  @brief Tests for user-requested mounts
 */

#include <string>
#include <sys/mount.h>
#include <sys/stat.h>

#include "aux/filesystem.hpp"
#include "aux/unitTestMain.hpp"
#include "libsarus/Mount.hpp"
#include "libsarus/PathRAII.hpp"
#include "libsarus/Utility.hpp"


namespace libsarus {
namespace test {

TEST_GROUP(MountTestGroup) {
};

#ifdef ASROOT
TEST(MountTestGroup, mount_test) {
#else
IGNORE_TEST(MountTestGroup, mount_test) {
#endif
    libsarus::UserIdentity userIdentity;

    auto bundleDirRAII = libsarus::PathRAII{libsarus::filesystem::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("test-bundle-dir"))};
    const auto& bundleDir = bundleDirRAII.getPath();
    auto rootfsDir = bundleDir / "rootfs";

    auto sourceDirRAII = libsarus::PathRAII{boost::filesystem::path{"./user_mounts_source"}};
    const auto& sourceDir = sourceDirRAII.getPath();
    auto destinationDir = boost::filesystem::path{"/user_mounts_destination"};

    auto sourceFile = libsarus::PathRAII{boost::filesystem::path{"./user_mounts_source_file"}};
    auto destinationFile = libsarus::PathRAII{boost::filesystem::path{"/user_mounts_destination_file"}};

    size_t mount_flags = 0;

    // Create files and directories
    libsarus::filesystem::createFoldersIfNecessary(rootfsDir);
    aux::filesystem::createTestDirectoryTree(sourceDir.string());
    libsarus::filesystem::createFileIfNecessary(sourceFile.getPath());
    auto command = "echo \"test data\" >" + sourceFile.getPath().string();
    auto ret = std::system(command.c_str());
    CHECK(WIFEXITED(ret) != 0 && WEXITSTATUS(ret) == 0);

    // test mount of non-existing destination directory
    {
        libsarus::Mount{sourceDir, destinationDir, mount_flags, rootfsDir, userIdentity}.performMount();
        CHECK(aux::filesystem::areDirectoriesEqual(sourceDir.string(), (rootfsDir / destinationDir).string(), 1));

        // cleanup
        CHECK(umount((rootfsDir / destinationDir).c_str()) == 0);
        boost::filesystem::remove_all(rootfsDir / destinationDir);
    }
    // test mount of existing destination directory
    {
        libsarus::filesystem::createFoldersIfNecessary(rootfsDir / destinationDir);
        libsarus::Mount{sourceDir, destinationDir.c_str(), mount_flags, rootfsDir, userIdentity}.performMount();
        CHECK(aux::filesystem::areDirectoriesEqual(sourceDir.string(), (rootfsDir / destinationDir).string(), 1));

        // cleanup
        CHECK(umount((rootfsDir / destinationDir).c_str()) == 0);
        boost::filesystem::remove_all(rootfsDir / destinationDir);
    }
    // test mount of individual file
    {
        libsarus::Mount{sourceFile.getPath(), destinationFile.getPath(), mount_flags, rootfsDir, userIdentity}.performMount();
        CHECK(aux::filesystem::isSameBindMountedFile(sourceFile.getPath(), rootfsDir / destinationFile.getPath()));

        // cleanup
        CHECK(umount((rootfsDir / destinationFile.getPath()).c_str()) == 0);
    }
    // test ctor with 5 arguments
    {
        libsarus::Mount{sourceFile.getPath(), destinationFile.getPath(), mount_flags, rootfsDir, userIdentity}.performMount();
        CHECK(aux::filesystem::isSameBindMountedFile(sourceFile.getPath(), rootfsDir / destinationFile.getPath()));

        // cleanup
        CHECK(umount((rootfsDir / destinationFile.getPath()).c_str()) == 0);
    }
    // test default move ctor
    {
        auto mountObject = libsarus::Mount{sourceFile.getPath(), destinationFile.getPath(), mount_flags, rootfsDir, userIdentity};
        libsarus::Mount{std::move(mountObject)}.performMount();
        CHECK(aux::filesystem::isSameBindMountedFile(sourceFile.getPath(), rootfsDir / destinationFile.getPath()));

        // cleanup
        CHECK(umount((rootfsDir / destinationFile.getPath()).c_str()) == 0);
    }
}

}}

SARUS_UNITTEST_MAIN_FUNCTION();
