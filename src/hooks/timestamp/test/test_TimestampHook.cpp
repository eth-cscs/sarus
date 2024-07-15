/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <vector>

#include <boost/regex.hpp>

#include "libsarus/PathRAII.hpp"
#include "libsarus/Utility.hpp"
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
    test_utility::config::ConfigRAII configRAII = test_utility::config::makeConfig();
    boost::filesystem::path bundleDir = configRAII.config->json["OCIBundleDir"].GetString();
    libsarus::PathRAII logFileRAII = libsarus::PathRAII{boost::filesystem::absolute("./timestamp_test.log")};
    boost::filesystem::path logFile = logFileRAII.getPath();
};

void createOCIBundleConfigJSON(const boost::filesystem::path& bundleDir, const std::string logVar, const std::tuple<uid_t, gid_t>& idsOfUser) {
    namespace rj = rapidjson;
    auto doc = test_utility::ocihooks::createBaseConfigJSON(bundleDir / "rootfs", idsOfUser);
    auto& allocator = doc.GetAllocator();
    if (!logVar.empty()) {
        doc["process"]["env"].PushBack(rj::Value{logVar.c_str(), allocator}, allocator);
    }

    libsarus::json::write(doc, bundleDir / "config.json");
}

TEST(TimestampTestGroup, test_disabled_hook) {
    createOCIBundleConfigJSON(bundleDir, "", idsOfUser);
    test_utility::ocihooks::writeContainerStateToStdin(bundleDir);

    // Create and call hook
    auto hook = TimestampHook{};
    hook.activate();
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
    libsarus::environment::setVariable("TIMESTAMP_HOOK_MESSAGE", expectedMessage);

    // Create and call hook
    auto hook = TimestampHook{};
    hook.activate();

    // Read logfile and check contents
    {
        auto logFileContent = libsarus::filesystem::readFile(logFile);

        auto initialPattern = "Line 1\nLine 2\n";
        auto timestampPattern = "\\[.*\\..*\\] \\[.*\\] \\[hook\\] \\[INFO\\] Timestamp hook: " + expectedMessage + "\n";
        auto expectedPattern = initialPattern + timestampPattern;

        auto regex = boost::regex(expectedPattern);
        boost::cmatch matches;
        CHECK(boost::regex_match(logFileContent.c_str(), matches, regex));
    }
}

TEST(TimestampTestGroup, test_non_existing_file) {
    auto logVariable = "TIMESTAMP_HOOK_LOGFILE=" + logFile.string();
    createOCIBundleConfigJSON(bundleDir, logVariable, idsOfUser);
    test_utility::ocihooks::writeContainerStateToStdin(bundleDir);

    // Set the expected message
    auto expectedMessage = std::string{"unit test"};
    libsarus::environment::setVariable("TIMESTAMP_HOOK_MESSAGE", expectedMessage);

    // Create and call hook
    auto hook = TimestampHook{};
    hook.activate();

    // Basic file checks
    CHECK(boost::filesystem::exists(logFile));
    CHECK(libsarus::filesystem::getOwner(logFile) == idsOfUser);

    // Read logfile and check contents
    {
        auto logFileContent = libsarus::filesystem::readFile(logFile);

        auto timestampPattern = "\\[.*\\..*\\] \\[.*\\] \\[hook\\] \\[INFO\\] Timestamp hook: " + expectedMessage + "\n";
        auto regex = boost::regex(timestampPattern);
        boost::cmatch matches;
        CHECK(boost::regex_match(logFileContent.c_str(), matches, regex));
    }
}

}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
