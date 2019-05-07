/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
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
    sarus::hooks::common::utility::enterNamespacesOfProcess(pidOfContainer);
    parseConfigJSONOfBundle();
    if(!isHookEnabled) {
        return;
    }
    parseEnvironmentVariables();
    timestamp();
}

void TimestampHook::parseConfigJSONOfBundle() {
    auto json = sarus::common::readJSON(bundleDir / "config.json");

    // get uid + gid of user
    uidOfUser = json["process"]["user"]["uid"].GetInt();
    gidOfUser = json["process"]["user"]["gid"].GetInt();

    // get environment variables
    for(const auto& v : json["process"]["env"].GetArray()) {
        std::string key, value;
        std::tie(key, value) = sarus::common::parseEnvironmentVariable(v.GetString());
        if(key == std::string{"TIMESTAMP_HOOK_LOGFILE"}) {
            isHookEnabled = true;
            logFilePath = std::move(value);
            break;
        }
    }
}

void TimestampHook::timestamp() {
    auto& logger = sarus::common::Logger::getInstance();
    logger.setLevel(sarus::common::logType::INFO);

    sarus::common::createFileIfNecessary(logFilePath, uidOfUser, gidOfUser);
    std::ofstream logFile(logFilePath.string(), std::ios::out | std::ios::app);

    auto fullMessage = boost::format("Timestamp hook: %s") % message;
    logger.log(fullMessage.str(), "hook", sarus::common::logType::INFO, logFile);
}

void TimestampHook::parseEnvironmentVariables() {
    char* p;
    if((p = getenv("TIMESTAMP_HOOK_MESSAGE")) != nullptr) {
        message = std::string{p};
    }
}


}}} // namespace
