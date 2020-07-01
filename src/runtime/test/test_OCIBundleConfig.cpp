/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <algorithm>
#include <string>
#include <fstream>

#include <boost/format.hpp>

#include "test_utility/config.hpp"
#include "common/PathRAII.hpp"
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
    config->commandRun.cpuAffinity = {0, 1, 2, 3};
    config->commandRun.execArgs = common::CLIArguments{"/bin/bash"};
    config->commandRun.addInitProcess = true;
    config->userIdentity.uid = 1000; // UID hardcoded in expected json file
    config->userIdentity.gid = 1000; // GID hardcoded in expected json file
    config->userIdentity.supplementaryGids = std::vector<gid_t>{2000, 3000, 4000, 1000}; // GIDs hardcoded in expected json file
    setGidOfTtyInEtcGroup(config, 5); // gid hardcoded in expected json file

    // create test bundle
    auto bundleDir = common::PathRAII{boost::filesystem::path{config->json["OCIBundleDir"].GetString()}};
    auto actualConfigFile = bundleDir.getPath() / "config.json";
    auto expectedConfigFile = boost::filesystem::path{__FILE__}.parent_path() / "expected_config.json";
    common::createFoldersIfNecessary(bundleDir.getPath());

    // run
    runtime::OCIBundleConfig{config}.generateConfigFile();

    // check
    CHECK(boost::filesystem::exists(actualConfigFile));
    auto expectedPermissions = boost::filesystem::perms::owner_read | boost::filesystem::perms::owner_write;
    CHECK_EQUAL(expectedPermissions, boost::filesystem::status(actualConfigFile).permissions())

    auto expectedJson = common::readJSON(expectedConfigFile);
    auto actualJson = common::readJSON(actualConfigFile);

    if(actualJson != expectedJson) {
        auto message =
            boost::format{"Generated config.json doesn't match expected config.json."
                          "\n\nEXPECTED:\n%s\n\nACTUAL:\n%s"}
            % common::readFile(expectedConfigFile)
            % common::readFile(actualConfigFile);
        std::cerr << message << std::endl;
        CHECK(false);
    }
}

SARUS_UNITTEST_MAIN_FUNCTION();
