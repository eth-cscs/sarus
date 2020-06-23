/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include <sstream>

#include "common/Utility.hpp"
#include "common/Config.hpp"
#include "common/PathRAII.hpp"
#include "runtime/OCIHooksFactory.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace runtime {
namespace test {

namespace rj = rapidjson;

TEST_GROUP(OCIHooksTestGroup) {
    common::PathRAII testDirRAII = common::PathRAII{ common::makeUniquePathWithRandomSuffix("test_oci_hook") };
    boost::filesystem::path jsonFile = testDirRAII.getPath() / "hook.json";
    boost::filesystem::path schemaFile = boost::filesystem::path{__FILE__}
        .parent_path()
        .parent_path()
        .parent_path()
        .parent_path() / "hook.schema.json";
};

TEST(OCIHooksTestGroup, create_hook_with_schema_incompatibility) {
    common::createFoldersIfNecessary(jsonFile.parent_path());

    // missing "stages" property
    {
        auto os = std::ofstream(jsonFile.c_str());
        os << "{"
        "   \"version\": \"1.0.0\","
        "   \"hook\": {"
        "       \"path\": \"/dir/test_hook\""
        "   },"
        "   \"when\": {"
        "       \"always\": true"
        "   }"
        "}";
        os.close();
        CHECK_THROWS(common::Error, OCIHooksFactory{}.createHook(jsonFile, schemaFile));
    }
    // undesired "extra" property
    {
        auto os = std::ofstream(jsonFile.c_str());
        os << "{"
        "   \"version\": \"1.0.0\","
        "   \"hook\": {"
        "       \"path\": \"/dir/test_hook\""
        "   },"
        "   \"when\": {"
        "       \"always\": true"
        "   },"
        "   \"stages\": [\"prestart\"],"
        "   \"extra\": true"
        "}";
        os.close();
        CHECK_THROWS(common::Error, OCIHooksFactory{}.createHook(jsonFile, schemaFile));
    }
}

TEST(OCIHooksTestGroup, create_hook_with_bad_version) {
    common::createFoldersIfNecessary(jsonFile.parent_path());
    auto os = std::ofstream(jsonFile.c_str());
    os << "{"
    "   \"version\": \"2.0.0\","
    "   \"hook\": {"
    "       \"path\": \"/dir/test_hook\""
    "   },"
    "   \"when\": {"
    "       \"always\": true"
    "   },"
    "   \"stages\": [\"prestart\", \"poststop\"]"
    "}";
    os.close();

    CHECK_THROWS(common::Error, OCIHooksFactory{}.createHook(jsonFile, schemaFile));
}

// RapidJson doesn't support regex with escaped dots ('\.' sequence).
// Hopefully this will get fixed in some future release of RapidJson,
// then this test will fail and we will know that the fix occurred :)
TEST(OCIHooksTestGroup, create_hook_with_unsupported_regex) {
    common::createFoldersIfNecessary(jsonFile.parent_path());
    auto os = std::ofstream(jsonFile.c_str());
    os << "{"
    "   \"version\": \"1.0.0\","
    "   \"hook\": {"
    "       \"path\": \"/dir/test_hook\""
    "   },"
    "   \"when\": {"
    "       \"annotations\": {"
    "           \"^com\\.oci\\.hooks\\.test_hook\\.enabled$\": \"^true$\""
    "       }"
    "   },"
    "   \"stages\": [\"prestart\"]"
    "}";
    os.close();
    CHECK_THROWS(common::Error, OCIHooksFactory{}.createHook(jsonFile, schemaFile));
}

TEST(OCIHooksTestGroup, create_hook_and_check_members) {
    common::createFoldersIfNecessary(jsonFile.parent_path());
    auto os = std::ofstream(jsonFile.c_str());
    os << "{"
    "   \"version\": \"1.0.0\","
    "   \"hook\": {"
    "       \"path\": \"/dir/test_hook\","
    "       \"args\": [\"test_hook\", \"arg\"],"
    "       \"env\": ["
    "           \"KEY0=VALUE0\","
    "           \"KEY1=VALUE1\""
    "       ],"
    "       \"timeout\": 3"
    "   },"
    "   \"when\": {"
    "       \"always\": true,"
    "       \"annotations\": {"
    "           \"^com.oci.hooks.test_hook.enabled$\": \"^true$\""
    "       },"
    "       \"commands\": [\"regex0\", \"regex1\"],"
    "       \"hasBindMounts\": true"
    "   },"
    "   \"stages\": [\"prestart\", \"poststop\"]"
    "}";
    os.close();

    auto hook = OCIHooksFactory{}.createHook(jsonFile, schemaFile);

    CHECK(hook.jsonFile == jsonFile);

    CHECK_EQUAL(hook.version, std::string{"1.0.0"});

    CHECK(hook.jsonHook["path"].GetString() == std::string{"/dir/test_hook"});
    CHECK_EQUAL(hook.jsonHook["args"].GetArray().Size(), 2)
    CHECK_EQUAL(hook.jsonHook["args"].GetArray()[0].GetString(), std::string{"test_hook"});
    CHECK_EQUAL(hook.jsonHook["args"].GetArray()[1].GetString(), std::string{"arg"});
    CHECK_EQUAL(hook.jsonHook["env"].GetArray().Size(), 2);
    CHECK_EQUAL(hook.jsonHook["env"].GetArray()[0].GetString(), std::string{"KEY0=VALUE0"});
    CHECK_EQUAL(hook.jsonHook["env"].GetArray()[1].GetString(), std::string{"KEY1=VALUE1"});
    CHECK_EQUAL(hook.jsonHook["timeout"].GetInt(), 3);

    CHECK_EQUAL(hook.conditions.size(), 4);
    CHECK(dynamic_cast<OCIHook::ConditionAlways*>(hook.conditions[0].get()) != nullptr);
    CHECK(dynamic_cast<OCIHook::ConditionAnnotations*>(hook.conditions[1].get()) != nullptr);
    CHECK(dynamic_cast<OCIHook::ConditionCommands*>(hook.conditions[2].get()) != nullptr);
    CHECK(dynamic_cast<OCIHook::ConditionHasBindMounts*>(hook.conditions[3].get()) != nullptr);

    CHECK((hook.stages == std::vector<std::string>{"prestart", "poststop"}));
}

TEST(OCIHooksTestGroup, create_hook_and_check_activation) {
    common::createFoldersIfNecessary(jsonFile.parent_path());
    auto os = std::ofstream(jsonFile.c_str());
    os << "{"
    "   \"version\": \"1.0.0\","
    "   \"hook\": {"
    "       \"path\": \"/dir/test_hook\""
    "   },"
    "   \"when\": {"
    "       \"always\": true,"
    "       \"annotations\": {"
    "           \"^com.oci.hooks.test_hook.enabled$\": \"^true$\""
    "       },"
    "       \"commands\": [\".*/app0\"],"
    "       \"hasBindMounts\": true"
    "   },"
    "   \"stages\": [\"prestart\"]"
    "}";
    os.close();

    auto hook = OCIHooksFactory{}.createHook(jsonFile, schemaFile);

    // all activation conditions met
    {
        auto configRaii = test_utility::config::makeConfig();
        configRaii.config->commandRun.bundleAnnotations["com.oci.hooks.test_hook.enabled"] = "true";
        configRaii.config->commandRun.execArgs = { "./app0" };
        configRaii.config->commandRun.mounts.push_back(
            std::unique_ptr<Mount>{ new Mount{"/src", "/dst", 0, configRaii.config} }
        );
        CHECK(hook.isActive(configRaii.config));
    }

    // "annotations" condition not met
    {
        auto configRaii = test_utility::config::makeConfig();
        configRaii.config->commandRun.bundleAnnotations["com.oci.hooks.test_hook.enabled"] = "false";
        configRaii.config->commandRun.execArgs = { "./app0" };
        configRaii.config->commandRun.mounts.push_back(
            std::unique_ptr<Mount>{ new Mount{"/src", "/dst", 0, configRaii.config} }
        );
        CHECK(!hook.isActive(configRaii.config));
    }

    // "command" condition not met
    {
        auto configRaii = test_utility::config::makeConfig();
        configRaii.config->commandRun.bundleAnnotations["com.oci.hooks.test_hook.enabled"] = "true";
        configRaii.config->commandRun.execArgs = { "./xyz0123" };
        configRaii.config->commandRun.mounts.push_back(
            std::unique_ptr<Mount>{ new Mount{"/src", "/dst", 0, configRaii.config} }
        );
        CHECK(!hook.isActive(configRaii.config));
    }

    // "hasBindMounts" condition not met
    {
        auto configRaii = test_utility::config::makeConfig();
        configRaii.config->commandRun.bundleAnnotations["com.oci.hooks.test_hook.enabled"] = "true";
        configRaii.config->commandRun.execArgs = { "./app0" };
        configRaii.config->commandRun.mounts = {};
        CHECK(!hook.isActive(configRaii.config));
    }
}

TEST(OCIHooksTestGroup, condition_always) {
    auto configRaii = test_utility::config::makeConfig();

    // false
    {
        auto json = rj::Document{};
        json.SetBool(false);
        auto condition = OCIHooksFactory{}.createCondition("always", json);
        CHECK(condition->evaluate(configRaii.config) == false);
    }
    // true
    {
        auto json = rj::Document{};
        json.SetBool(true);
        auto condition = OCIHooksFactory{}.createCondition("always", json);
        CHECK(condition->evaluate(configRaii.config) == true);
    }
}

TEST(OCIHooksTestGroup, condition_annotations) {
    auto json = rj::Document{};
    auto& allocator = json.GetAllocator();
    json.SetObject();
    json.AddMember("^com\\.oci\\.hooks\\.test_hook\\.enabled$", rj::Value{"^true$"}, allocator);
    json.AddMember("^com\\.oci\\.hooks\\.test_hook\\.domain$", rj::Value{".*mpi.*"}, allocator);

    auto condition = OCIHooksFactory{}.createCondition("annotations", json);

    // 0 matches out of 2
    {
        auto configRaii = test_utility::config::makeConfig();
        CHECK(condition->evaluate(configRaii.config) == false);
    }
    // 1 matches out of 2
    {
        auto configRaii = test_utility::config::makeConfig();
        configRaii.config->commandRun.bundleAnnotations["com.oci.hooks.test_hook.enabled"] = "true";
        configRaii.config->commandRun.bundleAnnotations["com.oci.hooks.test_hook.domain"] = "cuda-related stuff";
        CHECK(condition->evaluate(configRaii.config) == false);
    }
    // 2 matches out of 2
    {
        auto configRaii = test_utility::config::makeConfig();
        configRaii.config->commandRun.bundleAnnotations["com.oci.hooks.test_hook.enabled"] = "true";
        configRaii.config->commandRun.bundleAnnotations["com.oci.hooks.test_hook.domain"] = "mpi-related stuff";
        CHECK(condition->evaluate(configRaii.config) == true);
    }
}

TEST(OCIHooksTestGroup, condition_commands) {
    auto json = rj::Document{};
    auto& allocator = json.GetAllocator();
    json.SetArray();
    json.PushBack(rj::Value{".*/app0"}, allocator);
    json.PushBack(rj::Value{".*/app1"}, allocator);

    auto condition = OCIHooksFactory{}.createCondition("commands", json);

    // no matches
    {
        auto configRaii = test_utility::config::makeConfig();
        configRaii.config->commandRun.execArgs = { "./another_app" };
        CHECK(condition->evaluate(configRaii.config) == false);
    }
    // regex0 matches
    {
        auto configRaii = test_utility::config::makeConfig();
        configRaii.config->commandRun.execArgs = {"/usr/bin/app0"};
        CHECK(condition->evaluate(configRaii.config) == true);
    }
    // regex1 matches
    {
        auto configRaii = test_utility::config::makeConfig();
        configRaii.config->commandRun.execArgs = {"/usr/bin/app1"};
        CHECK(condition->evaluate(configRaii.config) == true);
    }
}

TEST(OCIHooksTestGroup, condition_hasBindMounts) {
    // no bind mounts
    {
        auto configRaii = test_utility::config::makeConfig();

        // hasBindMounts=false
        {
            auto json = rj::Document{};
            json.SetBool(false);
            auto condition = OCIHooksFactory{}.createCondition("hasBindMounts", json);

            CHECK(condition->evaluate(configRaii.config) == true);
        }
        // hasBindMount=true
        {
            auto json = rj::Document{};
            json.SetBool(true);
            auto condition = OCIHooksFactory{}.createCondition("hasBindMounts", json);

            CHECK(condition->evaluate(configRaii.config) == false);
        }
    }
    // bind mounts
    {
        auto configRaii = test_utility::config::makeConfig();
        configRaii.config->commandRun.mounts.push_back(
            std::unique_ptr<Mount>{ new Mount{"/src", "/dst", 0, configRaii.config} }
        );

        // hasBindMounts=false
        {
            auto json = rj::Document{};
            json.SetBool(false);
            auto condition = OCIHooksFactory{}.createCondition("hasBindMounts", json);

            CHECK(condition->evaluate(configRaii.config) == false);
        }
        // hasBindMounts=true
        {
            auto json = rj::Document{};
            json.SetBool(true);
            auto condition = OCIHooksFactory{}.createCondition("hasBindMounts", json);

            CHECK(condition->evaluate(configRaii.config) == true);
        }
    }
}

}}} // namespaces

SARUS_UNITTEST_MAIN_FUNCTION();
