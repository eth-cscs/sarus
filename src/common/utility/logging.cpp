/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "logging.hpp"

#include "common/Logger.hpp"

/**
 * Utility functions for output controls 
 */

namespace sarus {
namespace common {

void logMessage(const boost::format& message, LogLevel level, std::ostream& out, std::ostream& err) {
    logMessage(message.str(), level, out, err);
}

void logMessage(const std::string& message, LogLevel level, std::ostream& out, std::ostream& err) {
    auto subsystemName = "CommonUtility";
    common::Logger::getInstance().log(message, subsystemName, level, out, err);
}

} // namespace
} // namespace
