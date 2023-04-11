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

#include "common/Utility.hpp"
#include "common/Logger.hpp"
#include "hooks/common/Utility.hpp"


namespace sarus {
namespace hooks {
namespace timestamp {

void TimestampHook::activate() {
    std::tie(bundleDir, pidOfContainer) = hooks::common::utility::parseStateOfContainerFromStdin();
    parseConfigJSONOfBundle();
    if(!isHookEnabled) {
        return;
    }
    parseEnvironmentVariables();
    timestamp();
}

void TimestampHook::parseConfigJSONOfBundle() {
    auto json = sarus::common::readJSON(bundleDir / "config.json");

    hooks::common::utility::applyLoggingConfigIfAvailable(json);

    // get uid + gid of user
    uidOfUser = json["process"]["user"]["uid"].GetInt();
    gidOfUser = json["process"]["user"]["gid"].GetInt();

    // get environment variables
    auto env = hooks::common::utility::parseEnvironmentVariablesFromOCIBundle(bundleDir);
    if(env.find("TIMESTAMP_HOOK_LOGFILE") != env.cend()) {
        logFilePath = env["TIMESTAMP_HOOK_LOGFILE"];
        isHookEnabled = true;
    }
}

void TimestampHook::timestamp() {
    auto& logger = sarus::common::Logger::getInstance();
    logger.setLevel(sarus::common::LogLevel::INFO);

    sarus::common::createFileIfNecessary(logFilePath, uidOfUser, gidOfUser);
    std::ofstream logFile(logFilePath.string(), std::ios::out | std::ios::app);

    auto fullMessage = boost::format("Timestamp hook: %s") % message;
    logger.log(fullMessage.str(), "hook", sarus::common::LogLevel::INFO, logFile);
}

void TimestampHook::parseEnvironmentVariables() {
    char* p;
    if((p = getenv("TIMESTAMP_HOOK_MESSAGE")) != nullptr) {
        message = std::string{p};
    }
}


}}} // namespace
