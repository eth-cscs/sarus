/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "hooks/timestamp/TimestampHook.hpp"

#include <string>
#include <fstream>

#include "libsarus/Utility.hpp"
#include "libsarus/Logger.hpp"


namespace sarus {
namespace hooks {
namespace timestamp {

void TimestampHook::activate() {
    containerState = libsarus::hook::parseStateOfContainerFromStdin();
    parseConfigJSONOfBundle();
    if(!isHookEnabled) {
        return;
    }
    parseEnvironmentVariables();
    timestamp();
}

void TimestampHook::parseConfigJSONOfBundle() {
    auto json = libsarus::json::read(containerState.bundle() / "config.json");

    libsarus::hook::applyLoggingConfigIfAvailable(json);

    // get uid + gid of user
    uidOfUser = json["process"]["user"]["uid"].GetInt();
    gidOfUser = json["process"]["user"]["gid"].GetInt();

    // get environment variables
    auto env = libsarus::hook::parseEnvironmentVariablesFromOCIBundle(containerState.bundle());
    if(env.find("TIMESTAMP_HOOK_LOGFILE") != env.cend()) {
        logFilePath = env["TIMESTAMP_HOOK_LOGFILE"];
        isHookEnabled = true;
    }
}

void TimestampHook::timestamp() {
    auto& logger = libsarus::Logger::getInstance();
    logger.setLevel(libsarus::LogLevel::INFO);

    libsarus::filesystem::createFileIfNecessary(logFilePath, uidOfUser, gidOfUser);
    std::ofstream logFile(logFilePath.string(), std::ios::out | std::ios::app);

    auto fullMessage = boost::format("Timestamp hook: %s") % message;
    logger.log(fullMessage.str(), "hook", libsarus::LogLevel::INFO, logFile);
}

void TimestampHook::parseEnvironmentVariables() {
    char* p;
    if((p = getenv("TIMESTAMP_HOOK_MESSAGE")) != nullptr) {
        message = std::string{p};
    }
}


}}} // namespace
