/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "environment.hpp"

#include "common/Error.hpp"
#include "common/utility/logging.hpp"
#include "common/utility/string.hpp"

/**
 * Utility functions for environment variables
 */

namespace sarus {
namespace common {

std::unordered_map<std::string, std::string> parseEnvironmentVariables(char** env) {
    auto map = std::unordered_map<std::string, std::string>{};
    for(size_t i=0; env[i] != nullptr; ++i) {
        std::string key, value;
        std::tie(key, value) = parseEnvironmentVariable(env[i]);
        map[key] = value;
    }
    return map;
}

std::pair<std::string, std::string> parseEnvironmentVariable(const std::string& variable) {
    std::pair<std::string, std::string> kvPair;
    try {
        kvPair = common::parseKeyValuePair(variable);
    }
    catch(const common::Error& e) {
        auto message = boost::format("Failed to parse environment variable: %s") % e.what();
        SARUS_RETHROW_ERROR(e, message.str());
    }
    return kvPair;
}

std::string getEnvironmentVariable(const std::string& key) {
    char* p;
    if((p = getenv(key.c_str())) == nullptr) {
        auto message = boost::format("Environment doesn't contain variable with key %s") % key;
        SARUS_THROW_ERROR(message.str());
    }
    logMessage(boost::format("Got environment variable %s=%s") % key % p, common::LogLevel::DEBUG);
    return p;
}

void setEnvironmentVariable(const std::string& key, const std::string& value) {
    int overwrite = 1;
    if(setenv(key.c_str(), value.c_str(), overwrite) != 0) {
        auto message = boost::format("Failed to setenv(%s, %s, %d): %s")
            % key % value % overwrite % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    logMessage(boost::format("Set environment variable %s=%s") % key % value, common::LogLevel::DEBUG);
}

void clearEnvironmentVariables() {
    if(clearenv() != 0) {
        SARUS_THROW_ERROR("Failed to clear host environment variables");
    }

    auto path = std::string{"/bin:/sbin:/usr/bin"};
    int overwrite = 1;
    if(setenv("PATH", path.c_str(), overwrite) != 0) {
        auto message = boost::format("Failed to setenv(\"PATH\", %s, %d): %s")
            % path.c_str() % overwrite % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}

} // namespace
} // namespace
