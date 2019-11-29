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
#include "common/GroupDB.hpp"
#include "runtime/OCIBundleConfig.hpp"
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(OCIBundleConfigTestGroup) {
};

void setGidOfTtyInEtcGroup(const std::shared_ptr<common::Config>& config, gid_t gid) {
    auto prefixDir = boost::filesystem::path{config->json["prefixDir"].GetString()};
    auto group = common::GroupDB{};
    group.read(prefixDir / "etc/group");
    auto& entries = group.getEntries();

    auto isTtyGroup = [](const common::GroupDB::Entry& entry){
        return entry.groupName == "tty";
    };
    auto it = std::find_if( entries.begin(), entries.end(), isTtyGroup);

    if(it != entries.cend()) {
        it->gid = gid;
    }
    else {
        auto entry = common::GroupDB::Entry{"tty", "x", gid, std::vector<std::string>{}};
        entries.push_back(std::move(entry));
    }

    group.write(prefixDir / "etc/group");
}

TEST(OCIBundleConfigTestGroup, OCIBundleConfig) {
    // create test config
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    config->commandRun.execArgs = common::CLIArguments{"/bin/bash"};
    config->commandRun.addInitProcess = true;
    config->userIdentity.uid = 1000; // UID hardcoded in expected json file
    config->userIdentity.gid = 1000; // GID hardcoded in expected json file
    config->userIdentity.supplementaryGids = std::vector<gid_t>{2000, 3000, 4000, 1000}; // GIDs hardcoded in expected json file
    setGidOfTtyInEtcGroup(config, 5); // gid hardcoded in expected json file
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
    auto expectedPermissions = boost::filesystem::perms::owner_read | boost::filesystem::perms::owner_write;
    CHECK_EQUAL(expectedPermissions, boost::filesystem::status(actualConfigFile).permissions())
    auto expectedContent = common::removeWhitespaces(common::readFile(expectedConfigFile));
    auto actualContent = common::removeWhitespaces(common::readFile(actualConfigFile));
    CHECK_EQUAL(expectedContent, actualContent);

    // cleanup
    boost::filesystem::remove_all(bundleDir);
    boost::filesystem::remove(metadataFile);
}

SARUS_UNITTEST_MAIN_FUNCTION();
