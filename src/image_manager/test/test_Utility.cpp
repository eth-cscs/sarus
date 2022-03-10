/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/Logger.hpp"
#include "image_manager/Utility.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace image_manager {
namespace test {

TEST_GROUP(ImageManagerUtilityTestGroup) {
};

TEST(ImageManagerUtilityTestGroup, getSkopeoVerbosityOption) {
    auto& logger = common::Logger::getInstance();
    logger.setLevel(common::LogLevel::DEBUG);
    CHECK(utility::getSkopeoVerbosityOption() == std::string{"--debug"});

    logger.setLevel(common::LogLevel::INFO);
    CHECK(utility::getSkopeoVerbosityOption() == std::string{});

    logger.setLevel(common::LogLevel::WARN);
    CHECK(utility::getSkopeoVerbosityOption() == std::string{});

    logger.setLevel(common::LogLevel::ERROR);
    CHECK(utility::getSkopeoVerbosityOption() == std::string{});
}

TEST(ImageManagerUtilityTestGroup, getUmociVerbosityOption) {
    auto& logger = common::Logger::getInstance();
    logger.setLevel(common::LogLevel::DEBUG);
    CHECK(utility::getUmociVerbosityOption() == std::string{"--log=debug"});

    logger.setLevel(common::LogLevel::INFO);
    CHECK(utility::getUmociVerbosityOption() == std::string{"--log=info"});

    logger.setLevel(common::LogLevel::WARN);
    CHECK(utility::getUmociVerbosityOption() == std::string{"--log=error"});

    logger.setLevel(common::LogLevel::ERROR);
    CHECK(utility::getUmociVerbosityOption() == std::string{"--log=error"});
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
