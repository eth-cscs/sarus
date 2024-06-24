/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "libsarus/Logger.hpp"
#include "libsarus/CLIArguments.hpp"
#include "image_manager/UmociDriver.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace image_manager {
namespace test {

TEST_GROUP(UmociDriverTestGroup) {
};

TEST(UmociDriverTestGroup, generateBaseArgs) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    auto driver = image_manager::UmociDriver{config};
    auto& logger = libsarus::Logger::getInstance();

    auto expectedUmociPath = std::string(config->json["umociPath"].GetString());

    logger.setLevel(libsarus::LogLevel::DEBUG);
    auto umociArgs = driver.generateBaseArgs();
    auto expectedArgs = libsarus::CLIArguments{expectedUmociPath, "--log=debug"};
    CHECK(umociArgs == expectedArgs);

    logger.setLevel(libsarus::LogLevel::INFO);
    umociArgs = driver.generateBaseArgs();
    expectedArgs = libsarus::CLIArguments{expectedUmociPath, "--log=info"};
    CHECK(umociArgs == expectedArgs);

    logger.setLevel(libsarus::LogLevel::WARN);
    umociArgs = driver.generateBaseArgs();
    expectedArgs = libsarus::CLIArguments{expectedUmociPath, "--log=error"};
    CHECK(umociArgs == expectedArgs);

    logger.setLevel(libsarus::LogLevel::ERROR);
    umociArgs = driver.generateBaseArgs();
    expectedArgs = libsarus::CLIArguments{expectedUmociPath, "--log=error"};
    CHECK(umociArgs == expectedArgs);
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
