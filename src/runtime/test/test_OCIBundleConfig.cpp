/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <algorithm>
#include <string>
#include <fstream>

#include "test_utility/config.hpp"
#include "common/Utility.hpp"
#include "common/Logger.hpp"
#include "runtime/OCIBundleConfig.hpp"
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(OCIBundleConfigTestGroup) {
};

TEST(OCIBundleConfigTestGroup, OCIBundleConfig) {
    // create test config
    auto config = std::make_shared<common::Config>(test_utility::config::makeConfig());
    config->commandRun.execArgs = common::CLIArguments{"/bin/bash"};
    config->userIdentity.uid = 1000; // UID hardcoded in expected json file
    config->userIdentity.gid = 1000; // GID hardcoded in expected json file
    config->userIdentity.supplementaryGids = std::vector<gid_t>{2000, 3000, 4000, 1000}; // GIDs hardcoded in expected json file
    config->imageID = common::ImageID{"test", "test", "test", "test_image"};

    // create test bundle
    auto bundleDir = boost::filesystem::path{config->json["OCIBundleDir"].GetString()};
    auto actualConfigFile = bundleDir / "config.json";
    auto expectedConfigFile = boost::filesystem::path{__FILE__}.parent_path() / "expected_config.json";
    common::createFoldersIfNecessary(bundleDir);

    // create dummy metadata file in image repo
    auto metadataFile = boost::filesystem::path(config->directories.images / (config->imageID.getUniqueKey() + ".meta"));
    common::createFileIfNecessary(metadataFile);
    std::ofstream metadataStream(metadataFile.c_str());
    metadataStream << "{}";
    metadataStream.close();

    // run
    runtime::OCIBundleConfig{config}.generateConfigFile();

    // check
    CHECK(boost::filesystem::exists(actualConfigFile));
    auto expected = common::removeWhitespaces(common::readFile(expectedConfigFile));
    auto actual = common::removeWhitespaces(common::readFile(actualConfigFile));
    CHECK_EQUAL(expected, actual);

    // cleanup
    boost::filesystem::remove_all(bundleDir);
    boost::filesystem::remove_all(config->directories.repository);
}

SARUS_UNITTEST_MAIN_FUNCTION();
