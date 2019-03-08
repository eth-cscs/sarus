#include <boost/filesystem.hpp>
#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

#include <vector>

#include "common/Utility.hpp"
#include "hooks/timestamp/TimestampHook.hpp"
#include "test_utility/Misc.hpp"
#include "test_utility/config.hpp"
#include "test_utility/OCIHooks.hpp"
#include "test_utility/unittest_main_function.hpp"


namespace sarus {
namespace hooks {
namespace timestamp {
namespace test {

TEST_GROUP(TimestampTestGroup) {
    std::tuple<uid_t, gid_t> idsOfUser = test_utility::misc::getNonRootUserIds();
    sarus::common::Config config = test_utility::config::makeConfig();
    boost::filesystem::path bundleDir = boost::filesystem::path{ config.json.get()["OCIBundleDir"].GetString() };
    boost::filesystem::path logFile   = boost::filesystem::path{boost::filesystem::absolute("./timestamp_test.log")};
};

void createOCIBundleConfigJSON(const boost::filesystem::path& bundleDir, const std::string logVar, const std::tuple<uid_t, gid_t>& idsOfUser) {
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
    if (!logVar.empty()) {
        doc["process"]["env"].PushBack(rj::Value{logVar.c_str(), allocator}, allocator);
    }

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

TEST(TimestampTestGroup, test_disabled_hook) {
    createOCIBundleConfigJSON(bundleDir, "", idsOfUser);
    test_utility::ocihooks::writeContainerStateToStdin(bundleDir);

    // Create and call hook
    auto hook = TimestampHook{"unit test"};
    hook.activate();

    // cleanup
    boost::filesystem::remove_all(bundleDir);
}

TEST(TimestampTestGroup, test_existing_file) {
    // Create test log file with some content
    {
        std::ofstream logFileStream(logFile.string());
        logFileStream << "Line 1\nLine 2\n";
    }

    auto logVariable = "TIMESTAMP_HOOK_LOGFILE=" + logFile.string();
    createOCIBundleConfigJSON(bundleDir, logVariable, idsOfUser);
    test_utility::ocihooks::writeContainerStateToStdin(bundleDir);

    // Create and call hook
    auto hook = TimestampHook{"$unit test"};
    hook.activate();

    // Open logfile and check contents
    {
        std::vector<std::string> fileLines;
        std::ifstream logFileStream(logFile.string());
        for (std::string line; std::getline(logFileStream, line); ) {
            fileLines.push_back(line);
        }

        CHECK(fileLines.size() == 3);
        CHECK(fileLines[0] == std::string("Line 1"));
        CHECK(fileLines[1] == std::string("Line 2"));
        auto timestampPreambleEnd = std::find(fileLines[2].cbegin(), fileLines[2].cend(), '$');
        CHECK(timestampPreambleEnd != fileLines[2].cend());
        auto timestampMessage = std::string(timestampPreambleEnd+1, fileLines[2].cend());
        CHECK(timestampMessage == std::string("unit test"));
    }

    // cleanup
    boost::filesystem::remove_all(bundleDir);
    boost::filesystem::remove_all(logFile);
}

TEST(TimestampTestGroup, test_non_existing_file) {
    auto logVariable = "TIMESTAMP_HOOK_LOGFILE=" + logFile.string();
    createOCIBundleConfigJSON(bundleDir, logVariable, idsOfUser);
    test_utility::ocihooks::writeContainerStateToStdin(bundleDir);

    // Create and call hook
    auto hook = TimestampHook{"$unit test"};
    hook.activate();

    // Basic file checks
    CHECK(boost::filesystem::exists(logFile));
    CHECK(sarus::common::getOwner(logFile) == idsOfUser);

    // Open logfile and check contents
    {
        std::vector<std::string> fileLines;
        std::ifstream logFileStream(logFile.string());
        for (std::string line; std::getline(logFileStream, line); ) {
            fileLines.push_back(line);
        }

        CHECK(fileLines.size() == 1);
        auto timestampPreambleEnd = std::find(fileLines[0].cbegin(), fileLines[0].cend(), '$');
        CHECK(timestampPreambleEnd != fileLines[0].cend());
        auto timestampMessage = std::string(timestampPreambleEnd+1, fileLines[0].cend());
        CHECK(timestampMessage == std::string("unit test"));
    }

    // cleanup
    boost::filesystem::remove_all(bundleDir);
    boost::filesystem::remove_all(logFile);
}

}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
