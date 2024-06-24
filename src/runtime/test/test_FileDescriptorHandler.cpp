/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "test_utility/config.hpp"
#include "libsarus/Utility.hpp"
#include "runtime/FileDescriptorHandler.hpp"
#include "test_utility/unittest_main_function.hpp"


using namespace sarus;

TEST_GROUP(RuntimeTestGroup) {
};

std::vector<int> openFiles(const std::vector<boost::filesystem::path>& testFiles) {
    auto testFDs = std::vector<int>{};
    for (const auto& file : testFiles) {
        libsarus::createFileIfNecessary(file);
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

#ifdef ASROOT
TEST(RuntimeTestGroup, applyChangesToFdsAndEnvVariablesAndBundleAnnotations) {
#else
IGNORE_TEST(RuntimeTestGroup, applyChangesToFdsAndEnvVariablesAndBundleAnnotations) {
#endif
    // configure
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    auto prefixDir = boost::filesystem::path{config->json["prefixDir"].GetString()};

    // test files
    auto testDir = prefixDir / "test";
    auto testFiles = std::vector<boost::filesystem::path>{
        prefixDir / "test/file0",
        prefixDir / "test/file1",
        prefixDir / "test/file2"
    };

    // test base case (nothing to do)
    auto handler = runtime::FileDescriptorHandler{config};
    handler.applyChangesToFdsAndEnvVariablesAndBundleAnnotations();
    CHECK_EQUAL(0, handler.getExtraFileDescriptors());

    // test PMI_FD on lowest test fd
    auto testFDs = openFiles(testFiles);
    config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(testFDs[0]);
    handler = runtime::FileDescriptorHandler{config};
    handler.preservePMIFdIfAny();
    handler.applyChangesToFdsAndEnvVariablesAndBundleAnnotations();
    CHECK_EQUAL(1, handler.getExtraFileDescriptors());
    CHECK_EQUAL(std::to_string(3), config->commandRun.hostEnvironment["PMI_FD"]);
    CHECK_EQUAL(testFiles[0].string(), boost::filesystem::canonical("/proc/self/fd/3").string());
    closeFiles(testFDs);

    // test PMI_FD on highest test fd
    testFDs = openFiles(testFiles);
    config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(testFDs[2]);
    handler = runtime::FileDescriptorHandler{config};
    handler.preservePMIFdIfAny();
    handler.applyChangesToFdsAndEnvVariablesAndBundleAnnotations();
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
    handler.applyChangesToFdsAndEnvVariablesAndBundleAnnotations();
    CHECK_EQUAL(1, handler.getExtraFileDescriptors());
    CHECK_EQUAL(std::to_string(3), config->commandRun.hostEnvironment["PMI_FD"]);
    CHECK_EQUAL(testFiles[2].string(), boost::filesystem::canonical("/proc/self/fd/3").string());
    closeFiles(testFDs);
}

SARUS_UNITTEST_MAIN_FUNCTION();
