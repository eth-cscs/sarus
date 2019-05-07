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

TEST_GROUP(SlurmGlobalSyncTestGroup) {
    std::tuple<uid_t, gid_t> idsOfUser = test_utility::misc::getNonRootUserIds();
    sarus::common::Config config = test_utility::config::makeConfig();
    boost::filesystem::path bundleDir = boost::filesystem::path{ config.json.get()["OCIBundleDir"].GetString() };
    boost::filesystem::path rootfsDir = bundleDir / config.json.get()["rootfsFolder"].GetString();
    boost::filesystem::path localRepositoryBaseDir = boost::filesystem::absolute(
        sarus::common::makeUniquePathWithRandomSuffix("./sarus-test-localrepositorybase"));
    boost::filesystem::path localRepositoryDir = sarus::common::getLocalRepositoryDirectory(localRepositoryBaseDir, std::get<0>(idsOfUser));
    boost::filesystem::path syncDir = localRepositoryDir / "slurm_global_sync/slurm-jobid-256";
};

void createOCIBundleConfigJSON(const boost::filesystem::path& bundleDir, const boost::filesystem::path& rootfsDir, const std::tuple<uid_t, gid_t>& idsOfUser) {
    namespace rj = rapidjson;
    auto doc = sarus::hooks::common::test::createBaseConfigJSON(rootfsDir, idsOfUser);
    auto& allocator = doc.GetAllocator();
    doc["process"]["env"].PushBack(rj::Value{"SLURM_JOB_ID=256", allocator}, allocator);
    doc["process"]["env"].PushBack(rj::Value{"SLURM_PROCID=0", allocator}, allocator);
    doc["process"]["env"].PushBack(rj::Value{"SLURM_NTASKS=2", allocator}, allocator); 

    try {
        sarus::common::writeJSON(doc, bundleDir / "config.json");
    }
    catch(const std::exception& e) {
        auto message = boost::format("Failed to write OCI Bundle's JSON configuration");
        SARUS_RETHROW_ERROR(e, message.str());
    }
}

TEST(SlurmGlobalSyncTestGroup, test_high_level_synchronization) {
    createOCIBundleConfigJSON(bundleDir, rootfsDir, idsOfUser);
    sarus::common::setEnvironmentVariable("SARUS_LOCAL_REPOSITORY_BASE_DIR=" + localRepositoryBaseDir.string());
    test_utility::ocihooks::writeContainerStateToStdin(bundleDir);

    // simulate arrival + departure of other process
    sarus::common::createFileIfNecessary(syncDir / "arrival/slurm-procid-1");
    sarus::common::createFileIfNecessary(syncDir / "departure/slurm-procid-1");

    // perform synchronization
    auto hook = Hook{};
    hook.performSynchronization();

    // cleanup
    boost::filesystem::remove_all(bundleDir);
    boost::filesystem::remove_all(localRepositoryBaseDir);
}

TEST(SlurmGlobalSyncTestGroup, test_internals) {
    createOCIBundleConfigJSON(bundleDir, rootfsDir, idsOfUser);
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

    // cleanup
    boost::filesystem::remove_all(bundleDir);
    boost::filesystem::remove_all(localRepositoryBaseDir);
}

}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
