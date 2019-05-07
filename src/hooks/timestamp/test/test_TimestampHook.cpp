#include <vector>

#include <boost/regex.hpp>

#include "common/Utility.hpp"
#include "hooks/common/Utility.hpp"
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
    auto doc = sarus::hooks::common::test::createBaseConfigJSON(bundleDir / "rootfs", idsOfUser);
    auto& allocator = doc.GetAllocator();
    if (!logVar.empty()) {
        doc["process"]["env"].PushBack(rj::Value{logVar.c_str(), allocator}, allocator);
    }

    try {
        sarus::common::writeJSON(doc, bundleDir / "config.json");
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
    auto hook = TimestampHook{};
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

    // Prepare bundle and container stdin
    auto logVariable = "TIMESTAMP_HOOK_LOGFILE=" + logFile.string();
    createOCIBundleConfigJSON(bundleDir, logVariable, idsOfUser);
    test_utility::ocihooks::writeContainerStateToStdin(bundleDir);

    // Set the expected message
    auto expectedMessage = std::string{"unit test"};
    sarus::common::setEnvironmentVariable("TIMESTAMP_HOOK_MESSAGE=" + expectedMessage);

    // Create and call hook
    auto hook = TimestampHook{};
    hook.activate();

    // Read logfile and check contents
    {
        auto logFileContent = sarus::common::readFile(logFile);

        auto initialPattern = "Line 1\nLine 2\n";
        auto timestampPattern = "\\[.*\\..*\\] \\[.*\\] \\[hook\\] \\[INFO\\] Timestamp hook: " + expectedMessage + "\n";
        auto expectedPattern = initialPattern + timestampPattern;

        auto regex = boost::regex(expectedPattern);
        boost::cmatch matches;
        CHECK(boost::regex_match(logFileContent.c_str(), matches, regex));
    }

    // cleanup
    boost::filesystem::remove_all(bundleDir);
    boost::filesystem::remove_all(logFile);
}

TEST(TimestampTestGroup, test_non_existing_file) {
    auto logVariable = "TIMESTAMP_HOOK_LOGFILE=" + logFile.string();
    createOCIBundleConfigJSON(bundleDir, logVariable, idsOfUser);
    test_utility::ocihooks::writeContainerStateToStdin(bundleDir);

    // Set the expected message
    auto expectedMessage = std::string{"unit test"};
    sarus::common::setEnvironmentVariable("TIMESTAMP_HOOK_MESSAGE=" + expectedMessage);

    // Create and call hook
    auto hook = TimestampHook{};
    hook.activate();

    // Basic file checks
    CHECK(boost::filesystem::exists(logFile));
    CHECK(sarus::common::getOwner(logFile) == idsOfUser);

    // Read logfile and check contents
    {
        auto logFileContent = sarus::common::readFile(logFile);

        auto timestampPattern = "\\[.*\\..*\\] \\[.*\\] \\[hook\\] \\[INFO\\] Timestamp hook: " + expectedMessage + "\n";
        auto regex = boost::regex(timestampPattern);
        boost::cmatch matches;
        CHECK(boost::regex_match(logFileContent.c_str(), matches, regex));
    }

    // cleanup
    boost::filesystem::remove_all(bundleDir);
    boost::filesystem::remove_all(logFile);
}

}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
