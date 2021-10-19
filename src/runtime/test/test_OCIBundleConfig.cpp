/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <algorithm>
#include <string>
#include <fstream>

#include <boost/format.hpp>
#include <rapidjson/document.h>

#include "test_utility/config.hpp"
#include "common/PathRAII.hpp"
#include "common/Utility.hpp"
#include "common/Logger.hpp"
#include "common/GroupDB.hpp"
#include "runtime/OCIBundleConfig.hpp"
#include "test_utility/Misc.hpp"
#include "test_utility/filesystem.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace rj = rapidjson;
using namespace sarus;

TEST_GROUP(OCIBundleConfigTestGroup) {
};

static void setGidOfTtyInEtcGroup(const std::shared_ptr<common::Config>& config, gid_t gid) {
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

static void setEnvironmentInJSONConfig(rj::Document& config) {
    auto& allocator = config.GetAllocator();

    rj::Value environmentValue(rj::kObjectType);
    rj::Value setValue(rj::kObjectType);
    setValue.AddMember("SARUS_CONFIG_SET",
                       rj::Value{"config_set_value", allocator},
                       allocator);
    rj::Value prependValue(rj::kObjectType);
    prependValue.AddMember("SARUS_CONFIG_PREPEND_APPEND",
                           rj::Value{"config_prepend_value", allocator},
                           allocator);
    rj::Value appendValue(rj::kObjectType);
    appendValue.AddMember("SARUS_CONFIG_PREPEND_APPEND",
                          rj::Value{"config_append_value", allocator},
                          allocator);
    rj::Value unsetValue(rj::kArrayType);
    unsetValue.PushBack("SARUS_CONFIG_UNSET", allocator);
    environmentValue.AddMember("set", setValue, allocator);
    environmentValue.AddMember("prepend", prependValue, allocator);
    environmentValue.AddMember("append", appendValue, allocator);
    environmentValue.AddMember("unset", unsetValue, allocator);
    config.AddMember("environment", environmentValue, allocator);
}

static void setupTestConfig(std::shared_ptr<common::Config>& config) {
    config->commandRun.cpuAffinity = {0, 1, 2, 3};
    config->commandRun.execArgs = common::CLIArguments{"/bin/bash"};
    config->commandRun.addInitProcess = true;
    config->userIdentity.uid = getuid();
    config->userIdentity.gid = getgid();
    config->userIdentity.supplementaryGids = std::vector<gid_t>{2000, 3000, 4000, 1000}; // GIDs hardcoded in expected json file
    setGidOfTtyInEtcGroup(config, 5); // gid hardcoded in expected json file
    setEnvironmentInJSONConfig(config->json);
}

static common::PathRAII createTestBundle(const std::shared_ptr<common::Config>& config) {
    auto bundleDir = common::PathRAII{boost::filesystem::path{config->json["OCIBundleDir"].GetString()}};
    common::createFoldersIfNecessary(bundleDir.getPath());
    return bundleDir;
}

static void checkExistenceAndPermissions(const boost::filesystem::path& file) {
    CHECK(boost::filesystem::exists(file));
    auto expectedPermissions = boost::filesystem::perms::owner_read | boost::filesystem::perms::owner_write;
    CHECK_EQUAL(expectedPermissions, boost::filesystem::status(file).permissions());
}

/**
/* Sort environment arrays in both JSON objects to avoid test failures due to how the elements
/* are ordered in the Sarus-generated JSON: inside Sarus the container environment is stored
/* as an unordered map, so the order of the elements when converted to a linear JSON array
/* is compiler-dependant.
/* The sorting does not compromise the test since what is important here is if the individual
/* elements are all there and they match, not which order they are in.
 */
static void sortJsonEnvironmentArray(rj::Document& json) {
    auto rjStringCompare = [](const rapidjson::Value& lhs, const rapidjson::Value& rhs) {
        return std::string(lhs.GetString()) < std::string(rhs.GetString());
    };
    std::sort(json["process"]["env"].Begin(), json["process"]["env"].End(), rjStringCompare);
}

static rj::Document getExpectedJson() {
    auto expectedConfigFile = boost::filesystem::path{__FILE__}.parent_path() / "expected_config.json";
    auto expectedJson = common::readJSON(expectedConfigFile);
    expectedJson["process"]["user"]["uid"] = int(getuid());
    expectedJson["process"]["user"]["gid"] = int(getgid());
    sortJsonEnvironmentArray(expectedJson);
    return expectedJson;
}

static void compareJsonObjects(const rj::Document& actualJson, const rj::Document& expectedJson) {
    if(actualJson != expectedJson) {
        auto message =
            boost::format{"Generated config.json doesn't match expected config.json."
                          "\n\nEXPECTED:\n%s\n\nACTUAL:\n%s"}
            % test_utility::misc::prettyPrintJSON(expectedJson)
            % test_utility::misc::prettyPrintJSON(actualJson);
        std::cerr << message << std::endl;
        CHECK(false);
    }
}

static void enterTestDeviceFilesInConfig(const common::PathRAII& bundleDir, const std::shared_ptr<common::Config> config) {
    auto testDevice0 = bundleDir.getPath() / "testDevice0";
    auto testDevice0MajorID = 500u; auto testDevice0MinorID = 500u;
    test_utility::filesystem::createCharacterDeviceFile(testDevice0, testDevice0MajorID, testDevice0MinorID);

    auto testDevice1 = bundleDir.getPath() / "testDevice1";
    auto testDevice1MajorID = 501u; auto testDevice1MinorID = 501u;
    test_utility::filesystem::createCharacterDeviceFile(testDevice1, testDevice1MajorID, testDevice1MinorID);

    // enter devices into config struct
    size_t mount_flags = 0;
    auto deviceVector = std::vector<std::shared_ptr<runtime::DeviceMount>>{};
    deviceVector.emplace_back(new runtime::DeviceMount{testDevice0, testDevice0, mount_flags, common::DeviceAccess("rwm"), config});
    deviceVector.emplace_back(new runtime::DeviceMount{testDevice1, testDevice1, mount_flags, common::DeviceAccess("rw"), config});
    config->commandRun.deviceMounts = deviceVector;
}

static void enterTestDeviceFilesInJson(rapidjson::Document* json) {
    auto* allocator = &json->GetAllocator();
    auto testDevice0Rule = rapidjson::Value{rapidjson::kObjectType};
    testDevice0Rule.AddMember("allow",  rapidjson::Value{true}, *allocator);
    testDevice0Rule.AddMember("type",   rapidjson::Value{"c"}, *allocator);
    testDevice0Rule.AddMember("major",  rapidjson::Value{500}, *allocator);
    testDevice0Rule.AddMember("minor",  rapidjson::Value{500}, *allocator);
    testDevice0Rule.AddMember("access", rapidjson::Value{"rwm"}, *allocator);
    (*json)["linux"]["resources"]["devices"].PushBack(testDevice0Rule, *allocator);
    auto testDevice1Rule = rapidjson::Value{rapidjson::kObjectType};
    testDevice1Rule.AddMember("allow",  rapidjson::Value{true}, *allocator);
    testDevice1Rule.AddMember("type",   rapidjson::Value{"c"}, *allocator);
    testDevice1Rule.AddMember("major",  rapidjson::Value{501}, *allocator);
    testDevice1Rule.AddMember("minor",  rapidjson::Value{501}, *allocator);
    testDevice1Rule.AddMember("access", rapidjson::Value{"rw"}, *allocator);
    (*json)["linux"]["resources"]["devices"].PushBack(testDevice1Rule, *allocator);
}

TEST(OCIBundleConfigTestGroup, OCIBundleConfig) {
    // create test config
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    setupTestConfig(config);

    auto bundleDir = createTestBundle(config);

    // run
    runtime::OCIBundleConfig{config}.generateConfigFile();

    // check existence and permissions of bundle's config.json
    auto actualConfigFile = bundleDir.getPath() / "config.json";
    checkExistenceAndPermissions(actualConfigFile);
    auto actualJson = common::readJSON(actualConfigFile);
    sortJsonEnvironmentArray(actualJson);

    compareJsonObjects(actualJson, getExpectedJson());
}

#ifdef ASROOT
TEST (OCIBundleConfigTestGroup, allowed_devices)  {
#else
IGNORE_TEST(OCIBundleConfigTestGroup, allowed_devices) {
#endif
    // create test config
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    setupTestConfig(config);

    auto bundleDir = createTestBundle(config);
    enterTestDeviceFilesInConfig(bundleDir, config);

    // run
    runtime::OCIBundleConfig{config}.generateConfigFile();

    // check existence and permissions of bundle's config.json
    auto actualConfigFile = bundleDir.getPath() / "config.json";
    checkExistenceAndPermissions(actualConfigFile);
    auto actualJson = common::readJSON(actualConfigFile);
    sortJsonEnvironmentArray(actualJson);

    auto expectedJson = getExpectedJson();
    enterTestDeviceFilesInJson(&expectedJson);

    compareJsonObjects(actualJson, expectedJson);
}

SARUS_UNITTEST_MAIN_FUNCTION();
