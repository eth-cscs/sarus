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

#ifdef NOTROOT
IGNORE_TEST(RuntimeTestGroup, prepareFileDescriptorsToPreserve) {
#else
TEST(RuntimeTestGroup, prepareFileDescriptorsToPreserve) {
#endif
    // configure
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    // create FileDescriptorHandler object
    auto handler = runtime::FileDescriptorHandler{config};

    // create test files
    auto prefixDir = boost::filesystem::path{config->json["prefixDir"].GetString()};
    auto testDir = prefixDir / "test";
    common::createFoldersIfNecessary(testDir);
    common::createFileIfNecessary(testDir / "file0");
    common::createFileIfNecessary(testDir / "file1");
    common::createFileIfNecessary(testDir / "file2");

    // open files with Linux functions, in order to get file descriptors
    auto testFD0 = open(boost::filesystem::path{testDir/"file0"}.string().c_str(), O_RDONLY);
    auto testFD1 = open(boost::filesystem::path{testDir/"file1"}.string().c_str(), O_RDONLY);
    auto testFD2 = open(boost::filesystem::path{testDir/"file2"}.string().c_str(), O_RDONLY);
    CHECK_EQUAL(testFD0, 3);
    CHECK_EQUAL(testFD1, 4);
    CHECK_EQUAL(testFD2, 5);

    // test base case (nothing to do)
    handler.prepareFileDescriptorsToPreserve();
    CHECK_EQUAL(handler.getExtraFileDescriptors(), std::string("0"));

    // test PMI_FD
    config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(testFD2);
    handler.prepareFileDescriptorsToPreserve();
    CHECK_EQUAL(handler.getExtraFileDescriptors(), std::string("3"));
    CHECK_EQUAL(config->commandRun.hostEnvironment["PMI_FD"], std::string("5"));
    CHECK_EQUAL(close(6), 0); // cleanup duplicated file descriptor
    config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(testFD2); // reset environment variable

    // test close-on-exec, PMI_FD
    fcntl(testFD1, F_SETFD, FD_CLOEXEC);
    handler.prepareFileDescriptorsToPreserve();
    CHECK_EQUAL(handler.getExtraFileDescriptors(), std::string("2"));
    CHECK_EQUAL(config->commandRun.hostEnvironment["PMI_FD"], std::string("4"));
    CHECK_EQUAL(close(4), 0); // cleanup duplicated file descriptor
    config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(testFD2); // reset environment variable

    // test gap, PMI_FD (fd 4 should be free from previous test)
    handler.prepareFileDescriptorsToPreserve();
    CHECK_EQUAL(handler.getExtraFileDescriptors(), std::string("2"));
    CHECK_EQUAL(config->commandRun.hostEnvironment["PMI_FD"], std::string("4"));
    CHECK_EQUAL(close(4), 0); // cleanup duplicated file descriptor
    config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(testFD2); // reset environment variable

    // test gap, close-on-exec, PMI_FD (fd 4 should be free from previous test)
    fcntl(testFD0, F_SETFD, FD_CLOEXEC);
    handler.prepareFileDescriptorsToPreserve();
    CHECK_EQUAL(handler.getExtraFileDescriptors(), std::string("1"));
    CHECK_EQUAL(config->commandRun.hostEnvironment["PMI_FD"], std::string("3"));

    // cleanup
    close(3);
    close(5);
}

SARUS_UNITTEST_MAIN_FUNCTION();
