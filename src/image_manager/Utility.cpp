/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "image_manager/Utility.hpp"

#include "common/Logger.hpp"


namespace sarus {
namespace image_manager {
namespace utility {

std::string getSkopeoVerbosityOption() {
    auto logLevel = common::Logger::getInstance().getLevel();
    if (logLevel == common::LogLevel::DEBUG) {
        return std::string{"--debug"};
    }
    return std::string{};
}

std::string getUmociVerbosityOption() {
    auto logLevel = common::Logger::getInstance().getLevel();
    if (logLevel == common::LogLevel::DEBUG) {
        return std::string{"--log=debug"};
    }
    else if (logLevel == common::LogLevel::INFO) {
        return std::string{"--log=info"};
    }
    return std::string{"--log=error"};
}

} // namespace
} // namespace
} // namespace
