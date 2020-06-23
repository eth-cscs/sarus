/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Hook.hpp"

#include <memory>
#include <chrono>
#include <thread>
#include <iterator>
#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "common/Utility.hpp"
#include "common/PasswdDB.hpp"
#include "hooks/common/Utility.hpp"

namespace sarus {
namespace hooks {
namespace slurm_global_sync {

Hook::Hook() {
    log("Initializing hook", sarus::common::LogLevel::INFO);

    std::tie(bundleDir, pidOfContainer) = hooks::common::utility::parseStateOfContainerFromStdin();
    hooks::common::utility::enterNamespacesOfProcess(pidOfContainer);
    parseConfigJSONOfBundle();

    log("Successfully initialized hook", sarus::common::LogLevel::INFO);
}

void Hook::loadConfigs() {
    if(!isHookEnabled) {
        log("Not loading configuration (hook disabled)", sarus::common::LogLevel::INFO);
        return;
    }

    log("Loading configuration (based on environment variables)",
        sarus::common::LogLevel::INFO);

    auto baseDir = boost::filesystem::path{ sarus::common::getEnvironmentVariable("HOOK_BASE_DIR") };
    auto passwdFile = sarus::common::getEnvironmentVariable("PASSWD_FILE");
    auto username = sarus::common::PasswdDB{passwdFile}.getUsername(uidOfUser);
    syncDir = baseDir / username / ".oci-hooks/slurm-global-sync" / ("jobid-" + slurmJobID + "-stepid-" + slurmStepID);
    syncDirArrival = syncDir / "arrival";
    syncFileArrival = syncDirArrival / ("slurm-procid-" + slurmProcID);
    syncDirDeparture = syncDir / "departure";
    syncFileDeparture = syncDirDeparture / ("slurm-procid-" + slurmProcID);

    log(boost::format{"Sync file arrival: %s"} % syncFileArrival, sarus::common::LogLevel::DEBUG);
    log(boost::format{"Sync file departure: %s"} % syncFileDeparture, sarus::common::LogLevel::DEBUG);

    log("Successfully loaded configuration", sarus::common::LogLevel::INFO);
}

void Hook::performSynchronization() const {
    if(!isHookEnabled) {
        log("Not performing synchronization (hook disabled)", sarus::common::LogLevel::INFO);
        return;
    }

    log("Performing synchronization", sarus::common::LogLevel::INFO);

    synchronizeArrival();
    synchronizeDeparture();

    log("Successfully performed synchronization", sarus::common::LogLevel::INFO);
}

void Hook::synchronizeArrival() const {
    signalArrival();
    log("Waiting for arrival of all container instances", sarus::common::LogLevel::DEBUG);
    while(!allInstancesArrived()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    log("Successfully waited for arrival of all container instances", sarus::common::LogLevel::DEBUG);
}

void Hook::synchronizeDeparture() const {
    signalDeparture();
    if(slurmProcID == "0") {
        log("Waiting for departure of all container instances", sarus::common::LogLevel::DEBUG);
        while(!allInstancesDeparted()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        log("Successfully waited for departure of all container instances", sarus::common::LogLevel::DEBUG);
        cleanupSyncDir();
    }
}

void Hook::signalArrival() const {
    createSyncFile(syncFileArrival);
    log(boost::format{"Signalled arrival (created sync file %s)"} % syncFileArrival,
        sarus::common::LogLevel::DEBUG);
}

void Hook::signalDeparture() const {
    createSyncFile(syncFileDeparture);
    log(boost::format{"Signalled departure (created sync file %s)"} % syncFileDeparture,
        sarus::common::LogLevel::DEBUG);
}

void Hook::cleanupSyncDir() const {
    boost::filesystem::remove_all(syncDir);
    log(boost::format{"Cleaned up sync directory %s"} % syncDir,
        sarus::common::LogLevel::DEBUG);
}

bool Hook::allInstancesArrived() const {
    return countFilesInDirectory(syncDirArrival) == std::stoul(slurmNTasks);
}

bool Hook::allInstancesDeparted() const {
    return countFilesInDirectory(syncDirDeparture) == std::stoul(slurmNTasks);
}

void Hook::createSyncFile(const boost::filesystem::path& file) const {
    if(boost::filesystem::exists(file)) {
        auto message = boost::format(
            "internal error: attempted to create"
            " sync file %s, but it already exists") % file;
        SARUS_THROW_ERROR(message.str());
    }
    sarus::common::createFileIfNecessary(file, uidOfUser, gidOfUser);
}

size_t Hook::countFilesInDirectory(const boost::filesystem::path& directory) const {
    auto begin = boost::filesystem::directory_iterator{directory};
    auto end = boost::filesystem::directory_iterator{};
    return std::distance(begin, end);
}


void Hook::parseConfigJSONOfBundle() {
    auto json = sarus::common::readJSON(bundleDir / "config.json");

    hooks::common::utility::applyLoggingConfigIfAvailable(json);

    // get environment variables
    auto env = hooks::common::utility::parseEnvironmentVariablesFromOCIBundle(bundleDir);
    if(env.find("SLURM_JOB_ID") == env.cend()
        || env.find("SLURM_STEPID") == env.cend()
        || env.find("SLURM_NTASKS") == env.cend()
        || env.find("SLURM_PROCID") == env.cend()) {
        isHookEnabled = false;
        return;
    }
    slurmJobID = env["SLURM_JOB_ID"];
    slurmStepID = env["SLURM_STEPID"];
    slurmNTasks = env["SLURM_NTASKS"];
    slurmProcID = env["SLURM_PROCID"];

    // get uid + gid of user
    uidOfUser = json["process"]["user"]["uid"].GetInt();
    gidOfUser = json["process"]["user"]["gid"].GetInt();
}

void Hook::log(const std::string& message, sarus::common::LogLevel level) const {
    sarus::common::Logger::getInstance().log(message, "Slurm global sync hook", level);
}

void Hook::log(const boost::format& message, sarus::common::LogLevel level) const {
    sarus::common::Logger::getInstance().log(message.str(), "Slurm global sync hook", level);
}

}}} // namesapce
