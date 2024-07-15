/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <iostream>

#include "libsarus/Logger.hpp"
#include "libsarus/Utility.hpp"

int main(int argc, char* argv[]) {
    boost::filesystem::path bundleDir;
    auto containerState = libsarus::hook::parseStateOfContainerFromStdin();
    auto json = libsarus::json::read(containerState.bundle() / "config.json");
    libsarus::hook::applyLoggingConfigIfAvailable(json);

    std::cout << "hook's stdout" << std::endl;
    std::cerr << "hook's stderr" << std::endl;

    auto& logger = libsarus::Logger::getInstance();
    logger.log("hook's DEBUG log message", "stdout_stderr_test_hook", libsarus::LogLevel::DEBUG);
    logger.log("hook's INFO log message", "stdout_stderr_test_hook", libsarus::LogLevel::INFO);
    logger.log("hook's WARN log message", "stdout_stderr_test_hook", libsarus::LogLevel::WARN);
    logger.log("hook's ERROR log message", "stdout_stderr_test_hook", libsarus::LogLevel::ERROR);

    return 0;
}
