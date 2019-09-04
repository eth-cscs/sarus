/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/**
 *  @brief Tests for site mounts
 */

#include <sys/mount.h>

#include <boost/filesystem.hpp>

#include "test_utility/config.hpp"
#include "test_utility/filesystem.hpp"
#include "common/Utility.hpp"
#include "runtime/SiteMount.hpp"
#include "test_utility/unittest_main_function.hpp"


using namespace sarus;

TEST_GROUP(SiteMountsTestGroup) {
};


#ifdef NOTROOT
IGNORE_TEST(SiteMountsTestGroup, make_site_mount_test) {
#else
TEST(SiteMountsTestGroup, make_site_mount_test) {
#endif
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    auto bundleDir = boost::filesystem::path{config->json["OCIBundleDir"].GetString()};
    auto rootfsDir = bundleDir / boost::filesystem::path{config->json["rootfsFolder"].GetString()};
    common::createFoldersIfNecessary(bundleDir / "overlay/rootfs-lower");

    auto sourceDir = boost::filesystem::path{"./site_mounts_source"};
    auto destinationDir = boost::filesystem::path{"/site_mounts_destination"};

    auto sourceFile = boost::filesystem::path{"./site_mounts_source_file"};
    auto destinationFile = boost::filesystem::path{"/site_mounts_destination_file"};

    size_t mount_flags = 0;

    // create files and directories
    common::createFoldersIfNecessary(rootfsDir);
    test_utility::filesystem::create_test_directory_tree(sourceDir.string());
    auto command = "echo \"test data\" > " + sourceFile.string();
    auto ret = std::system(command.c_str());
    CHECK(WIFEXITED(ret) != 0 && WEXITSTATUS(ret) == 0);

    // mount non-existing destination directory
    runtime::SiteMount{sourceDir, destinationDir, mount_flags, config}.performMount();
    CHECK(test_utility::filesystem::are_directories_equal(sourceDir.c_str(), (rootfsDir / destinationDir).c_str(), 1));

    // cleanup
    CHECK(umount((rootfsDir / destinationDir).c_str()) == 0);
    boost::filesystem::remove_all(rootfsDir / destinationDir);

    // mount existing destination directory
    common::createFoldersIfNecessary(rootfsDir / destinationDir);
    runtime::SiteMount{sourceDir, destinationDir, mount_flags, config}.performMount();
    CHECK(test_utility::filesystem::are_directories_equal(sourceDir.string(), (rootfsDir / destinationDir).string(), 1));

    // cleanup
    CHECK(umount((rootfsDir / destinationDir).c_str()) == 0);
    boost::filesystem::remove_all(rootfsDir / destinationDir);

    // mount file
    runtime::SiteMount{sourceFile, destinationFile, mount_flags, config}.performMount();
    CHECK(test_utility::filesystem::isSameBindMountedFile(sourceFile, rootfsDir / destinationFile));

    // cleanup
    CHECK(umount((rootfsDir / destinationFile).c_str()) == 0);
    boost::filesystem::remove_all(bundleDir);
    boost::filesystem::remove_all(sourceDir);
}


SARUS_UNITTEST_MAIN_FUNCTION();
