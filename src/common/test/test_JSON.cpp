/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <memory>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>

#include "common/Logger.hpp"
#include "common/Config.hpp"
#include "test_utility/unittest_main_function.hpp"


static auto testSourceDir = boost::filesystem::path {__FILE__}.parent_path();
static auto projectRootDir = testSourceDir.parent_path().parent_path().parent_path();

TEST_GROUP(JSONTestGroup) {
};

TEST(JSONTestGroup, validFile) {
    boost::filesystem::path jsonFile(testSourceDir / "json/valid.json");
    boost::filesystem::path jsonSchemaFile(projectRootDir / "etc/sarus.schema.json");
    auto config = std::make_shared<sarus::common::Config>(jsonFile, jsonSchemaFile);

    CHECK_EQUAL(config->json["securityChecks"].GetBool(), false);
    CHECK_EQUAL(config->json["OCIBundleDir"].GetString(), std::string("/var/sarus/OCIBundleDir"));
    CHECK_EQUAL(config->json["rootfsFolder"].GetString(), std::string("rootfsFolder"));
    CHECK_EQUAL(config->json["prefixDir"].GetString(), std::string("/opt/sarus"));
    CHECK_EQUAL(config->json["tempDir"].GetString(), std::string("/tmp"));
    CHECK_EQUAL(config->json["localRepositoryBaseDir"].GetString(), std::string("/home"));
    CHECK_EQUAL(config->json["skopeoPath"].GetString(), std::string("/usr/bin/skopeo"));
    CHECK_EQUAL(config->json["umociPath"].GetString(), std::string("/usr/bin/umoci"));
    CHECK_EQUAL(config->json["mksquashfsPath"].GetString(), std::string("/usr/sbin/mksquashfs"));
    CHECK_EQUAL(config->json["mksquashfsOptions"].GetString(), std::string("-comp gzip -processors 4 -Xcompression-level 6"));
    CHECK_EQUAL(config->json["initPath"].GetString(), std::string("/opt/sarus/bin/init"));
    CHECK_EQUAL(config->json["runcPath"].GetString(), std::string("/usr/bin/runc.amd64"));
    CHECK_EQUAL(config->json["ramFilesystemType"].GetString(), std::string("tmpfs"));

    const rapidjson::Value& site_mounts = config->json["siteMounts"];
    CHECK_EQUAL(site_mounts[0]["type"].GetString(), std::string("bind"));
    CHECK_EQUAL(site_mounts[0]["source"].GetString(), std::string("/home"));
    CHECK_EQUAL(site_mounts[0]["destination"].GetString(), std::string("/home"));
    CHECK(site_mounts[0]["flags"].ObjectEmpty());

    const rapidjson::Value& environment = config->json["environment"];
    CHECK(environment["set"].HasMember("VAR_TO_SET_IN_CONTAINER"));
    CHECK_EQUAL(environment["set"]["VAR_TO_SET_IN_CONTAINER"].GetString(), std::string("value"));
    CHECK(environment["prepend"].HasMember("VAR_WITH_LIST_OF_PATHS_IN_CONTAINER"));
    CHECK_EQUAL(environment["prepend"]["VAR_WITH_LIST_OF_PATHS_IN_CONTAINER"].GetString(), std::string("/path/to/prepend"));
    CHECK(environment["append"].HasMember("VAR_WITH_LIST_OF_PATHS_IN_CONTAINER"));
    CHECK_EQUAL(environment["append"]["VAR_WITH_LIST_OF_PATHS_IN_CONTAINER"].GetString(), std::string("/path/to/append"));
    CHECK_EQUAL(environment["unset"].Size(), 2);
    CHECK_EQUAL(environment["unset"][0].GetString(), std::string("VAR_TO_UNSET_IN_CONTAINER_0"));
    CHECK_EQUAL(environment["unset"][1].GetString(), std::string("VAR_TO_UNSET_IN_CONTAINER_1"));

    const rapidjson::Value& user_mounts = config->json["userMounts"];
    CHECK_EQUAL(user_mounts["notAllowedPrefixesOfPath"].Size(), 2);
    CHECK_EQUAL(user_mounts["notAllowedPrefixesOfPath"][0].GetString(), std::string("/etc"));
    CHECK_EQUAL(user_mounts["notAllowedPrefixesOfPath"][1].GetString(), std::string("/var"));
    CHECK_EQUAL(user_mounts["notAllowedPaths"].Size(), 1);
    CHECK_EQUAL(user_mounts["notAllowedPaths"][0].GetString(), std::string("/opt"));

    CHECK_EQUAL(config->json["seccompProfile"].GetString(), std::string("/opt/sarus/etc/seccomp/default.json"));
    CHECK_EQUAL(config->json["apparmorProfile"].GetString(), std::string("sarus-default"));
    CHECK_EQUAL(config->json["selinuxLabel"].GetString(), std::string("system_u:system_r:svirt_sarus_t:s0:c124,c675"));
    CHECK_EQUAL(config->json["selinuxMountLabel"].GetString(), std::string("system_u:object_r:svirt_sarus_file_t:s0:c715,c811"));

    const rapidjson::Value& containersPolicy = config->json["containersPolicy"];
    CHECK_EQUAL(containersPolicy["path"].GetString(), std::string("/opt/sarus/etc/policy.json"));
    CHECK_EQUAL(containersPolicy["enforce"].GetBool(), false);

    CHECK_EQUAL(config->json["containersRegistries.dPath"].GetString(), std::string("/opt/sarus/etc/registries.d"));
    CHECK_EQUAL(config->json["defaultMPIType"].GetString(), std::string("mpich"));

    const rapidjson::Value& lockTimings = config->json["repositoryMetadataLockTimings"];
    CHECK_EQUAL(lockTimings["timeoutMs"].GetInt(), 120000);
    CHECK_EQUAL(lockTimings["warningMs"].GetInt(), 15000);
}

TEST(JSONTestGroup, minimumRequirementsFile) {
    boost::filesystem::path jsonFile(testSourceDir / "json/min_required.json");
    boost::filesystem::path jsonSchemaFile(projectRootDir / "etc/sarus.schema.json");
    sarus::common::Config{jsonFile, jsonSchemaFile};
}

TEST(JSONTestGroup, missingRequired) {
    boost::filesystem::path jsonFile(testSourceDir / "json/missing_required.json");
    boost::filesystem::path jsonSchemaFile(projectRootDir / "etc/sarus.schema.json");
    CHECK_THROWS(sarus::common::Error, sarus::common::Config(jsonFile, jsonSchemaFile));
}

TEST(JSONTestGroup, relativePaths) {
    boost::filesystem::path jsonFile(testSourceDir / "json/relative_paths.json");
    boost::filesystem::path jsonSchemaFile(projectRootDir / "etc/sarus.schema.json");
    CHECK_THROWS(sarus::common::Error, sarus::common::Config(jsonFile, jsonSchemaFile));
}

TEST(JSONTestGroup, siteMountWithoutType) {
    boost::filesystem::path jsonFile(testSourceDir / "json/site_mount_without_type.json");
    boost::filesystem::path jsonSchemaFile(projectRootDir / "etc/sarus.schema.json");
    CHECK_THROWS(sarus::common::Error, sarus::common::Config(jsonFile, jsonSchemaFile));
}

TEST(JSONTestGroup, invalidLockTiming) {
    boost::filesystem::path jsonFile(testSourceDir / "json/invlid_lock_timing.json");
    boost::filesystem::path jsonSchemaFile(projectRootDir / "etc/sarus.schema.json");
    CHECK_THROWS(sarus::common::Error, sarus::common::Config(jsonFile, jsonSchemaFile));
}

SARUS_UNITTEST_MAIN_FUNCTION();
