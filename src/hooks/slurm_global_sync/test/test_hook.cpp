#include <boost/filesystem.hpp>
#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

#include "common/Utility.hpp"
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
    boost::filesystem::path localRepositoryBaseDir = boost::filesystem::absolute(
        sarus::common::makeUniquePathWithRandomSuffix("./sarus-test-localrepositorybase"));
    boost::filesystem::path localRepositoryDir = sarus::common::getLocalRepositoryDirectory(localRepositoryBaseDir, std::get<0>(idsOfUser));
    boost::filesystem::path syncDir = localRepositoryDir / "slurm_global_sync/slurm-jobid-256";
};

void createOCIBundleConfigJSON(const boost::filesystem::path& bundleDir, const std::tuple<uid_t, gid_t>& idsOfUser) {
    namespace rj = rapidjson;
    auto doc = rj::Document{rj::kObjectType};
    auto& allocator = doc.GetAllocator();
    doc.AddMember(
        "process",
        rj::Document{rj::kObjectType},
        allocator);
    doc["process"].AddMember(
        "user",
        rj::Document{rj::kObjectType},
        allocator
    );
    doc["process"]["user"].AddMember(
        "uid",
        rj::Value{std::get<0>(idsOfUser)},
        allocator);
    doc["process"]["user"].AddMember(
        "gid",
        rj::Value{std::get<1>(idsOfUser)},
        allocator);
    doc["process"].AddMember(
        "env",
        rj::Document{rj::kArrayType},
        allocator);
    doc["process"]["env"].PushBack(rj::Value{"SLURM_JOB_ID=256", allocator}, allocator);
    doc["process"]["env"].PushBack(rj::Value{"SLURM_PROCID=0", allocator}, allocator);
    doc["process"]["env"].PushBack(rj::Value{"SLURM_NTASKS=2", allocator}, allocator); 

    try {
        sarus::common::createFoldersIfNecessary(bundleDir);
        std::ofstream ofs((bundleDir / "config.json").c_str());
        rj::OStreamWrapper osw(ofs);
        rj::Writer<rj::OStreamWrapper> writer(osw);
        doc.Accept(writer);
    }
    catch(const std::exception& e) {
        auto message = boost::format("Failed to write OCI Bundle's JSON configuration");
        SARUS_RETHROW_ERROR(e, message.str());
    }
}

TEST(SlurmGlobalSyncTestGroup, test_high_level_synchronization) {
    createOCIBundleConfigJSON(bundleDir, idsOfUser);
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
    createOCIBundleConfigJSON(bundleDir, idsOfUser);
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
