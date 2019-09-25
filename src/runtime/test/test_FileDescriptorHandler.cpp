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

std::vector<int> openFiles(const std::vector<boost::filesystem::path>& testFiles) {
    auto testFDs = std::vector<int>{};
    for (const auto& file : testFiles) {
        common::createFileIfNecessary(file);
        // open files with Linux functions, in order to get file descriptors
        testFDs.push_back(open(file.c_str(), O_RDONLY));
    }
    return testFDs;
}

void closeFiles(std::vector<int>& fds) {
    for(auto fd : fds) {
        if(fcntl(fd, F_GETFD) == -1) {
            continue; // invalid file descriptor
        }
        CHECK_EQUAL(0, close(fd));
    }
}

#ifdef NOTROOT
IGNORE_TEST(RuntimeTestGroup, applyChangesToFdsAndEnvVariables) {
#else
TEST(RuntimeTestGroup, applyChangesToFdsAndEnvVariables) {
#endif
    // configure
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    auto prefixDir = boost::filesystem::path{config->json["prefixDir"].GetString()};

    // open test files
    auto testDir = prefixDir / "test";
    auto testFiles = std::vector<boost::filesystem::path>{
        prefixDir / "test/file0",
        prefixDir / "test/file1",
        prefixDir / "test/file2"
    };

    // test base case (nothing to do)
    auto handler = runtime::FileDescriptorHandler{config};
    handler.applyChangesToFdsAndEnvVariables();
    CHECK_EQUAL(0, handler.getExtraFileDescriptors());

    // test PMI_FD on lowest test fd
    auto testFDs = openFiles(testFiles);
    config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(testFDs[0]);
    handler = runtime::FileDescriptorHandler{config};
    handler.preservePMIFdIfAny();
    handler.applyChangesToFdsAndEnvVariables();
    CHECK_EQUAL(1, handler.getExtraFileDescriptors());
    CHECK_EQUAL(std::to_string(3), config->commandRun.hostEnvironment["PMI_FD"]);
    CHECK_EQUAL(testFiles[0].string(), boost::filesystem::canonical("/proc/self/fd/3").string());
    closeFiles(testFDs);

    // test PMI_FD on highest test fd
    testFDs = openFiles(testFiles);
    config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(testFDs[2]);
    handler = runtime::FileDescriptorHandler{config};
    handler.preservePMIFdIfAny();
    handler.applyChangesToFdsAndEnvVariables();
    CHECK_EQUAL(1, handler.getExtraFileDescriptors());
    CHECK_EQUAL(std::to_string(3), config->commandRun.hostEnvironment["PMI_FD"]);
    CHECK_EQUAL(testFiles[2].string(), boost::filesystem::canonical("/proc/self/fd/3").string());
    closeFiles(testFDs);

    // test PMI_FD on highest test fd with gap (close fds before it)
    testFDs = openFiles(testFiles);
    CHECK_EQUAL(0, close(testFDs[0]));
    CHECK_EQUAL(0, close(testFDs[1]));
    config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(testFDs[2]);
    handler = runtime::FileDescriptorHandler{config};
    handler.preservePMIFdIfAny();
    handler.applyChangesToFdsAndEnvVariables();
    CHECK_EQUAL(1, handler.getExtraFileDescriptors());
    CHECK_EQUAL(std::to_string(3), config->commandRun.hostEnvironment["PMI_FD"]);
    CHECK_EQUAL(testFiles[2].string(), boost::filesystem::canonical("/proc/self/fd/3").string());
    closeFiles(testFDs);

    // test Sarus stdout stderr fds
    testFDs = openFiles(testFiles);
    handler = runtime::FileDescriptorHandler{config};
    handler.passStdoutAndStderrToHooks();
    handler.applyChangesToFdsAndEnvVariables();
    CHECK_EQUAL(2, handler.getExtraFileDescriptors());
    CHECK_EQUAL(std::to_string(3), config->commandRun.hooksEnvironment["SARUS_STDOUT_FD"]);
    CHECK_EQUAL(std::to_string(4), config->commandRun.hooksEnvironment["SARUS_STDERR_FD"]);
    CHECK(boost::filesystem::canonical("/proc/self/fd/1") == boost::filesystem::canonical("/proc/self/fd/3"));
    CHECK(boost::filesystem::canonical("/proc/self/fd/2") == boost::filesystem::canonical("/proc/self/fd/4"));

    // test Sarus stdout stderr + PMI
    testFDs = openFiles(testFiles);
    config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(testFDs[2]);
    handler = runtime::FileDescriptorHandler{config};
    handler.passStdoutAndStderrToHooks();
    handler.preservePMIFdIfAny();
    handler.applyChangesToFdsAndEnvVariables();
    CHECK_EQUAL(3, handler.getExtraFileDescriptors());
    CHECK_EQUAL(std::to_string(3), config->commandRun.hooksEnvironment["SARUS_STDOUT_FD"]);
    CHECK_EQUAL(std::to_string(4), config->commandRun.hooksEnvironment["SARUS_STDERR_FD"]);
    CHECK_EQUAL(std::to_string(5), config->commandRun.hostEnvironment["PMI_FD"]);
    CHECK(boost::filesystem::canonical("/proc/self/fd/1") == boost::filesystem::canonical("/proc/self/fd/3"));
    CHECK(boost::filesystem::canonical("/proc/self/fd/2") == boost::filesystem::canonical("/proc/self/fd/4"));
    CHECK_EQUAL(testFiles[2].string(), boost::filesystem::canonical("/proc/self/fd/5").string());
}

SARUS_UNITTEST_MAIN_FUNCTION();
