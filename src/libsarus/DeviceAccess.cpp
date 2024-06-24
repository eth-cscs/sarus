/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "DeviceAccess.hpp"
#include "Utility.hpp"

#include "libsarus/Error.hpp"

namespace libsarus {

DeviceAccess::DeviceAccess(const std::string& input) {
    parseInput(input);
}

void DeviceAccess::parseInput(const std::string& input) {
    if (input.empty()) {
        SARUS_THROW_ERROR("Input string for device access is empty");
    }

    if (input.size() > 3) {
        auto message = boost::format("Input string for device access '%s' is longer than 3 characters") % input;
        SARUS_THROW_ERROR(message.str());
    }

    for (const char& c : input) {
        if (c == 'r') {
            if (isReadAllowed()) {
                auto message = boost::format("Input string for device access '%s' has repeated characters") % input;
                SARUS_THROW_ERROR(message.str());
            }
            allowRead();
        }
        else if (c == 'w') {
            if (isWriteAllowed()) {
                auto message = boost::format("Input string for device access '%s' has repeated characters") % input;
                SARUS_THROW_ERROR(message.str());
            }
            allowWrite();
        }
        else if (c == 'm') {
            if (isMknodAllowed()) {
                auto message = boost::format("Input string for device access '%s' has repeated characters") % input;
                SARUS_THROW_ERROR(message.str());
            }
            allowMknod();
        }
        else {
            auto message = boost::format("Input string for device access '%s' contains an invalid character") % input;
            SARUS_THROW_ERROR(message.str());
        }
    }

    logMessage(boost::format("Correctly parsed device access permissions: %s") % string(), libsarus::LogLevel::DEBUG);
}

std::string DeviceAccess::string() const {
    auto output = std::string();

    if (isReadAllowed()) {
        output += 'r';
    }
    if (isWriteAllowed()) {
        output += 'w';
    }
    if (isMknodAllowed()) {
        output += 'm';
    }

    return output;
}

void DeviceAccess::logMessage(const boost::format& message, libsarus::LogLevel level,
                std::ostream& out, std::ostream& err) const {
    logMessage(message.str(), level, out, err);
}

void DeviceAccess::logMessage(const std::string& message, libsarus::LogLevel level,
                std::ostream& out, std::ostream& err) const {
    auto subsystemName = "DeviceAccess";
    libsarus::Logger::getInstance().log(message, subsystemName, level, out, err);
}

}
