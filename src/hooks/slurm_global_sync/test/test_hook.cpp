/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "hooks/common/Utility.hpp"
#include "hooks/slurm_global_sync/Hook.hpp"
#include "test_utility/Misc.hpp"
#include "test_utility/config.hpp"
#include "test_utility/OCIHooks.hpp"
#include "test_utility/unittest_main_function.hpp"


namespace sarus {
namespace hooks {
namespace slurm_global_sync {
namespace test {

test_utility::config::ConfigRAII makeConfig() {
    auto configRAII = test_utility::config::makeConfig();
    std::tie(configRAII.config->userIdentity.uid, configRAII.config->userIdentity.gid) = test_utility::misc::getNonRootUserIds();
    return configRAII;
}

TEST_GROUP(SlurmGlobalSyncTestGroup) {
    test_utility::config::ConfigRAII configRAII = makeConfig();
    std::tuple<uid_t, gid_t> idsOfUser = { configRAII.config->userIdentity.uid, configRAII.config->userIdentity.gid };
    boost::filesystem::path prefixDir = configRAII.config->json["prefixDir"].GetString();
    boost::filesystem::path bundleDir = configRAII.config->json["OCIBundleDir"].GetString();
    boost::filesystem::path rootfsDir = bundleDir / configRAII.config->json["rootfsFolder"].GetString();
    boost::filesystem::path localRepositoryBaseDir = configRAII.config->json["localRepositoryBaseDir"].GetString();
    boost::filesystem::path localRepositoryDir = sarus::common::getLocalRepositoryDirectory(*configRAII.config);
    boost::filesystem::path configJsonSchema = boost::filesystem::path{__FILE__}
        .parent_path()
        .parent_path()
        .parent_path()
        .parent_path()
        .parent_path() / "sarus.schema.json";
    boost::filesystem::path syncDir = localRepositoryDir / "slurm_global_sync/slurm-jobid-256";
};

void createSarusJSON(   const sarus::common::Config& config,
                        const boost::filesystem::path& configJsonSchema,
                        const boost::filesystem::path& prefixDir) {
    sarus::common::createFoldersIfNecessary(prefixDir / "etc");
    sarus::common::writeJSON(config.json, prefixDir / "etc/sarus.json");
    sarus::common::copyFile(configJsonSchema, prefixDir / "etc/sarus.schema.json");
}

void createOCIBundleConfigJSON( const boost::filesystem::path& bundleDir,
                                const boost::filesystem::path& rootfsDir,
                                const std::tuple<uid_t, gid_t>& idsOfUser,
                                bool generateSlurmEnvironmentVariables=true) {
    namespace rj = rapidjson;
    auto doc = test_utility::ocihooks::createBaseConfigJSON(rootfsDir, idsOfUser);
    auto& allocator = doc.GetAllocator();
    
    if(generateSlurmEnvironmentVariables) {
        doc["process"]["env"].PushBack(rj::Value{"SLURM_JOB_ID=256", allocator}, allocator);
        doc["process"]["env"].PushBack(rj::Value{"SLURM_PROCID=0", allocator}, allocator);
        doc["process"]["env"].PushBack(rj::Value{"SLURM_NTASKS=2", allocator}, allocator); 
    }

    try {
        sarus::common::writeJSON(doc, bundleDir / "config.json");
    }
    catch(const std::exception& e) {
        auto message = boost::format("Failed to write OCI Bundle's JSON configuration");
        SARUS_RETHROW_ERROR(e, message.str());
    }
}

TEST(SlurmGlobalSyncTestGroup, test_hook_disabled) {
    createSarusJSON(*configRAII.config, configJsonSchema, prefixDir);
    bool generateSlurmEnvironmentVariables = false;
    createOCIBundleConfigJSON(bundleDir, rootfsDir, idsOfUser, generateSlurmEnvironmentVariables);
    test_utility::ocihooks::writeContainerStateToStdin(bundleDir);

    auto hook = Hook{};
    hook.performSynchronization(); // let's simply verify that no errors occur
}

TEST(SlurmGlobalSyncTestGroup, test_high_level_synchronization) {
    createSarusJSON(*configRAII.config, configJsonSchema, prefixDir);
    createOCIBundleConfigJSON(bundleDir, rootfsDir, idsOfUser);
    sarus::common::setEnvironmentVariable("SARUS_PREFIX_DIR=" + prefixDir.string());
    sarus::common::setEnvironmentVariable("SARUS_LOCAL_REPOSITORY_BASE_DIR=" + localRepositoryBaseDir.string());
    test_utility::ocihooks::writeContainerStateToStdin(bundleDir);

    // simulate arrival + departure of other process
    sarus::common::createFileIfNecessary(syncDir / "arrival/slurm-procid-1");
    sarus::common::createFileIfNecessary(syncDir / "departure/slurm-procid-1");

    // perform synchronization
    auto hook = Hook{};
    hook.performSynchronization();
    hook.cleanupSyncDir();
}

TEST(SlurmGlobalSyncTestGroup, test_internals) {
    createSarusJSON(*configRAII.config, configJsonSchema, prefixDir);
    createOCIBundleConfigJSON(bundleDir, rootfsDir, idsOfUser);
    sarus::common::setEnvironmentVariable("SARUS_PREFIX_DIR=" + prefixDir.string());
    sarus::common::setEnvironmentVariable("SARUS_LOCAL_REPOSITORY_BASE_DIR=" + localRepositoryBaseDir.string());
    test_utility::ocihooks::writeContainerStateToStdin(bundleDir);

    auto hook = Hook{};

    // signal arrival
    CHECK(!boost::filesystem::exists(syncDir / "arrival/slurm-procid-0"));
    hook.signalArrival();
    CHECK(boost::filesystem::exists(syncDir / "arrival/slurm-procid-0"));
    CHECK(sarus::common::getOwner(syncDir / "arrival/slurm-procid-0") == idsOfUser);

    // arrival synchronization
    CHECK(hook.allInstancesArrived() == false);
    sarus::common::createFileIfNecessary(syncDir / "arrival/slurm-procid-1");
    CHECK(hook.allInstancesArrived() == true);

    // signal departure
    CHECK(!boost::filesystem::exists(syncDir / "departure/slurm-procid-0"));
    hook.signalDeparture();
    CHECK(boost::filesystem::exists(syncDir / "departure/slurm-procid-0"));
    CHECK(sarus::common::getOwner(syncDir / "departure/slurm-procid-0") == idsOfUser);

    // departure synchronization
    CHECK(hook.allInstancesDeparted() == false);
    sarus::common::createFileIfNecessary(syncDir / "departure/slurm-procid-1");
    CHECK(hook.allInstancesDeparted() == true);

    // cleanup of sync dir
    hook.cleanupSyncDir();
    CHECK(!boost::filesystem::exists(syncDir));
}

}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
