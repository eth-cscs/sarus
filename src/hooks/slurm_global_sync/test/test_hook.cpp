/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/PathRAII.hpp"
#include "common/PasswdDB.hpp"
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
    std::tuple<uid_t, gid_t> idsOfUser{ configRAII.config->userIdentity.uid, configRAII.config->userIdentity.gid };
    sarus::common::PathRAII prefixDir = sarus::common::PathRAII{configRAII.config->json["prefixDir"].GetString()};
    sarus::common::PathRAII bundleDir = sarus::common::PathRAII{configRAII.config->json["OCIBundleDir"].GetString()};
    boost::filesystem::path rootfsDir = bundleDir.getPath() / configRAII.config->json["rootfsFolder"].GetString();
    boost::filesystem::path passwdFile = prefixDir.getPath() / "etc/passwd";
    boost::filesystem::path syncBaseDir = prefixDir.getPath() / "sync-base-dir";
    boost::filesystem::path syncDir = syncBaseDir
                                      / sarus::common::PasswdDB{passwdFile}.getUsername(std::get<0>(idsOfUser))
                                      / ".oci-hooks/slurm-global-sync/jobid-256-stepid-32";
};

void createOCIBundleConfigJSON(const boost::filesystem::path& bundleDir,
                               const boost::filesystem::path& rootfsDir,
                               const std::tuple<uid_t, gid_t>& idsOfUser,
                               bool generateSlurmEnvironmentVariables=true) {
    namespace rj = rapidjson;
    auto doc = test_utility::ocihooks::createBaseConfigJSON(rootfsDir, idsOfUser);
    auto& allocator = doc.GetAllocator();

    if(generateSlurmEnvironmentVariables) {
        doc["process"]["env"].PushBack(rj::Value{"SLURM_JOB_ID=256", allocator}, allocator);
        doc["process"]["env"].PushBack(rj::Value{"SLURM_STEPID=32", allocator}, allocator);
        doc["process"]["env"].PushBack(rj::Value{"SLURM_PROCID=0", allocator}, allocator);
        doc["process"]["env"].PushBack(rj::Value{"SLURM_NTASKS=2", allocator}, allocator);
    }

    sarus::common::writeJSON(doc, bundleDir / "config.json");
}

TEST(SlurmGlobalSyncTestGroup, test_hook_disabled) {
    sarus::common::setEnvironmentVariable("PASSWD_FILE", passwdFile.string());
    sarus::common::setEnvironmentVariable("HOOK_BASE_DIR", syncBaseDir.string());

    bool generateSlurmEnvironmentVariables = false;
    createOCIBundleConfigJSON(bundleDir.getPath(), rootfsDir, idsOfUser,
                              generateSlurmEnvironmentVariables);
    test_utility::ocihooks::writeContainerStateToStdin(bundleDir.getPath());
    auto hook = Hook{};
    hook.performSynchronization(); // just verify that no errors occur
}

TEST(SlurmGlobalSyncTestGroup, test_high_level_synchronization) {
    sarus::common::setEnvironmentVariable("PASSWD_FILE", passwdFile.string());
    sarus::common::setEnvironmentVariable("HOOK_BASE_DIR", syncBaseDir.string());
    createOCIBundleConfigJSON(bundleDir.getPath(), rootfsDir, idsOfUser);
    test_utility::ocihooks::writeContainerStateToStdin(bundleDir.getPath());

    // simulate arrival + departure of other process
    sarus::common::createFileIfNecessary(syncDir / "arrival/slurm-procid-1");
    sarus::common::createFileIfNecessary(syncDir / "departure/slurm-procid-1");

    // perform synchronization
    auto hook = Hook{};
    hook.loadConfigs();
    hook.performSynchronization();
    hook.cleanupSyncDir();
}

TEST(SlurmGlobalSyncTestGroup, test_internals) {
    sarus::common::setEnvironmentVariable("PASSWD_FILE", passwdFile.string());
    sarus::common::setEnvironmentVariable("HOOK_BASE_DIR", syncBaseDir.string());
    createOCIBundleConfigJSON(bundleDir.getPath(), rootfsDir, idsOfUser);
    test_utility::ocihooks::writeContainerStateToStdin(bundleDir.getPath());

    auto hook = Hook{};
    hook.loadConfigs();

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
