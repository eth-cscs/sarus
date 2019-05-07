/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Utility.hpp"

namespace sarus {
namespace runtime {
namespace utility {

void logMessage(const boost::format& message, common::logType level) {
    logMessage(message.str(), level);
}

void logMessage(const std::string& message, common::logType level) {
    auto subsystemName = "runtime";
    common::Logger::getInstance().log(message, subsystemName, level);
}

} // namespace
} // namespace
} // namespace
