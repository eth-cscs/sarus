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

#include "common/Logger.hpp"
#include "common/Utility.hpp"

using namespace sarus;

int main(int argc, char* argv[]) {
    boost::filesystem::path bundleDir;
    pid_t pidOfContainer;
    auto containerState = common::hook::parseStateOfContainerFromStdin();
    auto json = sarus::common::readJSON(containerState.bundle() / "config.json");
    common::hook::applyLoggingConfigIfAvailable(json);

    std::cout << "hook's stdout" << std::endl;
    std::cerr << "hook's stderr" << std::endl;

    auto& logger = sarus::common::Logger::getInstance();
    logger.log("hook's DEBUG log message", "stdout_stderr_test_hook", sarus::common::LogLevel::DEBUG);
    logger.log("hook's INFO log message", "stdout_stderr_test_hook", sarus::common::LogLevel::INFO);
    logger.log("hook's WARN log message", "stdout_stderr_test_hook", sarus::common::LogLevel::WARN);
    logger.log("hook's ERROR log message", "stdout_stderr_test_hook", sarus::common::LogLevel::ERROR);

    return 0;
}
