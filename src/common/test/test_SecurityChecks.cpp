/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <tuple>
#include <memory>

#include "common/Utility.hpp"
#include "common/SecurityChecks.hpp"
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(SecurityChecksTestGroup) {
};

TEST(SecurityChecksTestGroup, checkThatPathIsUntamperable) {
    auto config = std::make_shared<common::Config>();
    config->buildTime.areRuntimeSecurityChecksEnabled = true;
    auto securityChecks = common::SecurityChecks{std::move(config)};

    std::tuple<uid_t, gid_t> rootIds{0, 0};
    std::tuple<uid_t, gid_t> nonRootIds{1000, 1000};

    auto testDirectory = common::makeUniquePathWithRandomSuffix("/tmp/sarus-securitychecks-test");
    common::createFoldersIfNecessary(testDirectory, std::get<0>(rootIds), std::get<1>(rootIds));

    // tamperable file
    {
        auto path = testDirectory / "tamperable-file";
        common::createFileIfNecessary(path, std::get<0>(nonRootIds), std::get<1>(nonRootIds));
        CHECK_THROWS(common::Error, securityChecks.checkThatPathIsUntamperable(path));
        boost::filesystem::remove(path);
    }

    // untamperable file
    {
        auto path = testDirectory / "untamperable-file";
        common::createFileIfNecessary(path, std::get<0>(rootIds), std::get<1>(rootIds));
        securityChecks.checkThatPathIsUntamperable(path);
        boost::filesystem::remove(path);
    }

    // tamperable folder
    {
        auto path = testDirectory / "tamperable-subfolder";
        common::createFoldersIfNecessary(path, std::get<0>(nonRootIds), std::get<1>(nonRootIds));
        CHECK_THROWS(common::Error, securityChecks.checkThatPathIsUntamperable(path));
        boost::filesystem::remove(path);
    }

    // untamperable folder
    {
        auto path = testDirectory / "untamperable-subfolder";
        common::createFoldersIfNecessary(path, std::get<0>(rootIds), std::get<1>(rootIds));
        securityChecks.checkThatPathIsUntamperable(path);
        boost::filesystem::remove(path);
    }

    // tamperable parent folder
    {
        auto parent = testDirectory / "tamperable-parent-folder";
        auto path = parent / "file";
        common::createFoldersIfNecessary(parent, std::get<0>(nonRootIds), std::get<1>(nonRootIds));
        common::createFileIfNecessary(path, std::get<0>(rootIds), std::get<1>(rootIds));
        CHECK_THROWS(common::Error, securityChecks.checkThatPathIsUntamperable(path));
        boost::filesystem::remove_all(parent);
    }

    // writability
    {
        auto path = testDirectory / "group-writable-file";
        common::createFileIfNecessary(path, std::get<0>(rootIds), std::get<1>(rootIds));

        // ok permissions
        boost::filesystem::permissions(path, boost::filesystem::owner_all
                                            | boost::filesystem::group_read
                                            | boost::filesystem::others_read);
        securityChecks.checkThatPathIsUntamperable(path);

        // group-writable file
        boost::filesystem::permissions(path, boost::filesystem::owner_all
                                            | boost::filesystem::group_read | boost::filesystem::group_write
                                            | boost::filesystem::others_read);
        CHECK_THROWS(common::Error, securityChecks.checkThatPathIsUntamperable(path));

        // others-writable file
        boost::filesystem::permissions(path, boost::filesystem::owner_all
                                            | boost::filesystem::group_read
                                            | boost::filesystem::others_read | boost::filesystem::others_write);
        CHECK_THROWS(common::Error, securityChecks.checkThatPathIsUntamperable(path));

        boost::filesystem::remove(path);
    }

    // verify that security check is recursive
    {
        auto subdir0 = testDirectory / "dir0";
        auto subdir1 = subdir0 / "dir1";
        common::createFoldersIfNecessary(subdir1, std::get<0>(rootIds), std::get<1>(rootIds));

        // tamperable subdirectory 
        securityChecks.checkThatPathIsUntamperable(subdir0);
        auto tamperableSubdir = subdir1 / "tamperable-dir";
        common::createFoldersIfNecessary(tamperableSubdir, std::get<0>(nonRootIds), std::get<1>(nonRootIds));
        CHECK_THROWS(common::Error, securityChecks.checkThatPathIsUntamperable(subdir0));
        boost::filesystem::remove(tamperableSubdir);

        // tamperable file
        securityChecks.checkThatPathIsUntamperable(subdir0);
        auto file = subdir1 / "tamperable-file";
        common::createFileIfNecessary(file, std::get<0>(nonRootIds), std::get<1>(nonRootIds));
        CHECK_THROWS(common::Error, securityChecks.checkThatPathIsUntamperable(subdir0));

        boost::filesystem::remove_all(subdir0);
    }

    boost::filesystem::remove_all(testDirectory);
}

SARUS_UNITTEST_MAIN_FUNCTION();
