/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "test_utility/config.hpp"
#include "common/Utility.hpp"
#include "runtime/FileDescriptorHandler.hpp"
#include "test_utility/unittest_main_function.hpp"


using namespace sarus;

TEST_GROUP(RuntimeTestGroup) {
};


std::vector<int> openTestFiles(boost::filesystem::path prefixDir) {
    auto testDir = prefixDir / "test";
    auto testFiles = std::vector<boost::filesystem::path>
        {testDir/"file0", testDir/"file1", testDir/"file2"};
    auto testFDs = std::vector<int>{};

    common::createFoldersIfNecessary(testDir);
    for (const auto& file : testFiles) {
        common::createFileIfNecessary(file);
        // open files with Linux functions, in order to get file descriptors
        testFDs.push_back(open(file.string().c_str(), O_RDONLY));
    }

    return testFDs;
}

#ifdef NOTROOT
IGNORE_TEST(RuntimeTestGroup, prepareFileDescriptorsToPreserve) {
#else
TEST(RuntimeTestGroup, prepareFileDescriptorsToPreserve) {
#endif
    // configure
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    auto prefixDir = boost::filesystem::path{config->json["prefixDir"].GetString()};

    // test base case (nothing to do)
    auto handler = runtime::FileDescriptorHandler{config};
    auto testFDs = openTestFiles(prefixDir);
    handler.prepareFileDescriptorsToPreserve();
    CHECK_EQUAL(0, handler.getExtraFileDescriptors());

    // test PMI_FD on lowest test fd
    config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(testFDs[0]);
    handler = runtime::FileDescriptorHandler{config};
    handler.prepareFileDescriptorsToPreserve();
    CHECK_EQUAL(1, handler.getExtraFileDescriptors());
    CHECK_EQUAL(std::to_string(3), config->commandRun.hostEnvironment["PMI_FD"]);
    CHECK_EQUAL(std::string("file0"), boost::filesystem::canonical("/proc/self/fd/3").filename().string());
    CHECK_EQUAL(0, close(3)); //cleanup

    // test PMI_FD on highest test fd
    testFDs = openTestFiles(prefixDir);
    config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(testFDs.back());
    handler = runtime::FileDescriptorHandler{config};
    handler.prepareFileDescriptorsToPreserve();
    CHECK_EQUAL(1, handler.getExtraFileDescriptors());
    CHECK_EQUAL(std::to_string(3), config->commandRun.hostEnvironment["PMI_FD"]);
    CHECK_EQUAL(std::string("file2"), boost::filesystem::canonical("/proc/self/fd/3").filename().string());
    CHECK_EQUAL(0, close(3)); //cleanup

    // test PMI_FD on highest test fd with gap (close fds before it)
    testFDs = openTestFiles(prefixDir);
    CHECK_EQUAL(0, close(testFDs[0]));
    CHECK_EQUAL(0, close(testFDs[1]));
    config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(testFDs.back());
    handler = runtime::FileDescriptorHandler{config};
    handler.prepareFileDescriptorsToPreserve();
    CHECK_EQUAL(1, handler.getExtraFileDescriptors());
    CHECK_EQUAL(std::to_string(3), config->commandRun.hostEnvironment["PMI_FD"]);
    CHECK_EQUAL(std::string("file2"), boost::filesystem::canonical("/proc/self/fd/3").filename().string());
    CHECK_EQUAL(0, close(3)); //cleanup
}

SARUS_UNITTEST_MAIN_FUNCTION();
