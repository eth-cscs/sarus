/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <tuple>
#include <memory>

#include "common/PathRAII.hpp"
#include "common/Utility.hpp"
#include "runtime/SecurityChecks.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(SecurityChecksTestGroup) {
};

TEST(SecurityChecksTestGroup, checkThatPathIsUntamperable) {
    auto configRAII = test_utility::config::makeConfig();
    configRAII.config->json["securityChecks"] = true;
    auto securityChecks = runtime::SecurityChecks{configRAII.config};

    std::tuple<uid_t, gid_t> rootIds{0, 0};
    std::tuple<uid_t, gid_t> nonRootIds{1000, 1000};

    auto testPathRAII = common::PathRAII{common::makeUniquePathWithRandomSuffix("/sarus-securitychecks-test")};
    const auto& testDirectory = testPathRAII.getPath();
    common::createFoldersIfNecessary(testDirectory, std::get<0>(rootIds), std::get<1>(rootIds));

    // non-existent file
    {
        auto path = testDirectory / "nonexistent-file";
        securityChecks.checkThatPathIsUntamperable(path);
    }
    // untamperable file
    {
        auto path = testDirectory / "untamperable-file";
        common::createFileIfNecessary(path, std::get<0>(rootIds), std::get<1>(rootIds));
        securityChecks.checkThatPathIsUntamperable(path);
    }
    {
        auto path = testDirectory / "untamperable-file-gidx";
        common::createFileIfNecessary(path, std::get<0>(rootIds), std::get<1>(nonRootIds));
        securityChecks.checkThatPathIsUntamperable(path);
    }

    // untamperable folder
    {
        auto path = testDirectory / "untamperable-subfolder";
        common::createFoldersIfNecessary(path, std::get<0>(rootIds), std::get<1>(rootIds));
        securityChecks.checkThatPathIsUntamperable(path);
    }
    {
        auto path = testDirectory / "untamperable-subfolder-gidx";
        common::createFoldersIfNecessary(path, std::get<0>(rootIds), std::get<1>(nonRootIds));
        securityChecks.checkThatPathIsUntamperable(path);
    }

    // tamperable parent folder
    {
        auto parent = testDirectory / "tamperable-parent-folder";
        auto path = parent / "file";
        common::createFoldersIfNecessary(parent, std::get<0>(nonRootIds), std::get<1>(nonRootIds));
        common::createFileIfNecessary(path, std::get<0>(rootIds), std::get<1>(rootIds));
        CHECK_THROWS(common::Error, securityChecks.checkThatPathIsUntamperable(path));
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
    }

    // verify that security checks run on directory's subpaths
    {
        auto subdir0 = common::PathRAII{testDirectory / "dir0"};
        const auto& subdir1 = subdir0.getPath() / "dir1";
        common::createFoldersIfNecessary(subdir1, std::get<0>(rootIds), std::get<1>(rootIds));
        securityChecks.checkThatPathIsUntamperable(subdir0.getPath());
        securityChecks.checkThatPathIsUntamperable(subdir1);

        // tamperable subdirectory
        auto tamperableSubdir = common::PathRAII{subdir1 / "tamperable-dir"};
        common::createFoldersIfNecessary(tamperableSubdir.getPath(), std::get<0>(nonRootIds), std::get<1>(nonRootIds));
        CHECK_THROWS(common::Error, securityChecks.checkThatPathIsUntamperable(subdir0.getPath()));

        // tamperable file
        auto file = subdir1 / "tamperable-file";
        common::createFileIfNecessary(file, std::get<0>(nonRootIds), std::get<1>(nonRootIds));
        CHECK_THROWS(common::Error, securityChecks.checkThatPathIsUntamperable(subdir0.getPath()));
    }

    // verify that security checks run on all path's parents.
    {
        auto path = common::PathRAII{testDirectory / "no" / "problem"};
        const auto& untamperable = path.getPath();
        common::createFoldersIfNecessary(untamperable, std::get<0>(rootIds), std::get<1>(rootIds));
        securityChecks.checkThatPathIsUntamperable(untamperable);

        auto untamperable2 = untamperable / "still" / "ok";
        common::createFoldersIfNecessary(untamperable2, std::get<0>(rootIds), std::get<1>(nonRootIds));
        securityChecks.checkThatPathIsUntamperable(untamperable2);

        auto tamperable = untamperable / "ouch";
        common::createFoldersIfNecessary(tamperable, std::get<0>(nonRootIds), std::get<1>(nonRootIds));
        CHECK_THROWS(common::Error, securityChecks.checkThatPathIsUntamperable(tamperable));

        auto tamperable2 = untamperable / "duh";
        common::createFoldersIfNecessary(tamperable2, std::get<0>(nonRootIds), std::get<1>(rootIds));
        CHECK_THROWS(common::Error, securityChecks.checkThatPathIsUntamperable(tamperable2));

        auto broken = tamperable / "tamperable" / "in" / "path";
        common::createFoldersIfNecessary(broken, std::get<0>(rootIds), std::get<1>(rootIds));
        CHECK_THROWS(common::Error, securityChecks.checkThatPathIsUntamperable(broken));

        auto broken2 = tamperable2 / "tamperable" / "in" / "path";
        common::createFoldersIfNecessary(broken2, std::get<0>(rootIds), std::get<1>(rootIds));
        CHECK_THROWS(common::Error, securityChecks.checkThatPathIsUntamperable(broken2));
    }
}

SARUS_UNITTEST_MAIN_FUNCTION();
