/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Utility.hpp"

namespace sarus {
namespace runtime {
namespace utility {

void logMessage(const boost::format& message, common::LogLevel level,
                std::ostream& out, std::ostream& err) {
    logMessage(message.str(), level, out, err);
}

void logMessage(const std::string& message, common::LogLevel level,
                std::ostream& out, std::ostream& err) {
    auto subsystemName = "runtime";
    common::Logger::getInstance().log(message, subsystemName, level, out, err);
}

} // namespace
} // namespace
} // namespace
