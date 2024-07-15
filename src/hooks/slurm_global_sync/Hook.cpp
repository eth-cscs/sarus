/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
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
#include <sys/prctl.h>

#include "libsarus/Error.hpp"
#include "libsarus/Logger.hpp"
#include "libsarus/Utility.hpp"
#include "libsarus/PasswdDB.hpp"

namespace sarus {
namespace hooks {
namespace slurm_global_sync {

Hook::Hook() {
    log("Initializing hook", libsarus::LogLevel::INFO);

    containerState = libsarus::hook::parseStateOfContainerFromStdin();
    parseConfigJSONOfBundle();

    log("Successfully initialized hook", libsarus::LogLevel::INFO);
}

void Hook::loadConfigs() {
    if (!isHookEnabled) {
        log("Not loading configuration (hook disabled)", libsarus::LogLevel::INFO);
        return;
    }

    log("Loading configuration (based on environment variables)",
        libsarus::LogLevel::INFO);

    auto baseDir = boost::filesystem::path{ libsarus::environment::getVariable("HOOK_BASE_DIR") };
    auto passwdFile = libsarus::environment::getVariable("PASSWD_FILE");
    auto username = libsarus::PasswdDB{passwdFile}.getUsername(uidOfUser);
    syncDir = baseDir / username / ".oci-hooks/slurm-global-sync" / ("jobid-" + slurmJobID + "-stepid-" + slurmStepID);
    syncDirArrival = syncDir / "arrival";
    syncFileArrival = syncDirArrival / ("slurm-procid-" + slurmProcID);
    syncDirDeparture = syncDir / "departure";
    syncFileDeparture = syncDirDeparture / ("slurm-procid-" + slurmProcID);

    log(boost::format{"Sync file arrival: %s"} % syncFileArrival, libsarus::LogLevel::DEBUG);
    log(boost::format{"Sync file departure: %s"} % syncFileDeparture, libsarus::LogLevel::DEBUG);

    log("Successfully loaded configuration", libsarus::LogLevel::INFO);
}

void Hook::performSynchronization() const {
    if (!isHookEnabled) {
        log("Not performing synchronization (hook disabled)", libsarus::LogLevel::INFO);
        return;
    }

    log("Performing synchronization", libsarus::LogLevel::INFO);

    synchronizeArrival();
    synchronizeDeparture();

    log("Successfully performed synchronization", libsarus::LogLevel::INFO);
}

void Hook::synchronizeArrival() const {
    signalArrival();
    log("Waiting for arrival of all container instances", libsarus::LogLevel::DEBUG);
    // TODO: Implement a timeout
    while(!allInstancesArrived()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    log("Successfully waited for arrival of all container instances", libsarus::LogLevel::DEBUG);
}

void Hook::synchronizeDeparture() const {
    signalDeparture();
    if (slurmProcID == "0") {
        log("Waiting for departure of all container instances", libsarus::LogLevel::DEBUG);
        // TODO: Implement a timeout
        while(!allInstancesDeparted()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        log("Successfully waited for departure of all container instances", libsarus::LogLevel::DEBUG);
        cleanupSyncDir();
    }
}

void Hook::signalArrival() const {
    createSyncFile(syncFileArrival);
    log(boost::format{"Signalled arrival (created sync file %s)"} % syncFileArrival,
        libsarus::LogLevel::DEBUG);
}

void Hook::signalDeparture() const {
    createSyncFile(syncFileDeparture);
    log(boost::format{"Signalled departure (created sync file %s)"} % syncFileDeparture,
        libsarus::LogLevel::DEBUG);
}

void Hook::cleanupSyncDir() const {
    boost::filesystem::remove_all(syncDir);
    log(boost::format{"Cleaned up sync directory %s"} % syncDir,
        libsarus::LogLevel::DEBUG);
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
    libsarus::filesystem::createFileIfNecessary(file, uidOfUser, gidOfUser);
}

size_t Hook::countFilesInDirectory(const boost::filesystem::path& directory) const {
    auto begin = boost::filesystem::directory_iterator{directory};
    auto end = boost::filesystem::directory_iterator{};
    return std::distance(begin, end);
}

void Hook::parseConfigJSONOfBundle() {
    auto json = libsarus::json::read(containerState.bundle() / "config.json");

    libsarus::hook::applyLoggingConfigIfAvailable(json);

    // get environment variables
    auto env = libsarus::hook::parseEnvironmentVariablesFromOCIBundle(containerState.bundle());

    if (env.find("SLURM_JOB_ID") == env.cend()
        || env.find("SLURM_STEPID") == env.cend()
        || env.find("SLURM_NTASKS") == env.cend()
        || env.find("SLURM_PROCID") == env.cend()) {
        isHookEnabled = false;
        log("Disabled hook because cannot find SLURM_* variables", libsarus::LogLevel::DEBUG);
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

void Hook::dropPrivileges() const {
    uid_t ruid, euid, suid;
    if (getresuid(&ruid, &euid, &suid) != 0) {
        SARUS_THROW_ERROR("getresuid failed");
    }
    if (euid != 0) {
        return;
    }

    if (setresgid(gidOfUser, gidOfUser, gidOfUser) != 0) {
        auto message = boost::format("Failed to setresgid(%1%, %1%, %1%): %2%") % gidOfUser % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    if (setresuid(uidOfUser, uidOfUser, uidOfUser) != 0) {
        auto message = boost::format("Failed to setresuid(%1%, %1%, %1%): %2%") % uidOfUser % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        auto message = boost::format("Failed to set no_new_privs bit: %s") % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}

void Hook::log(const std::string& message, libsarus::LogLevel level) const {
    libsarus::Logger::getInstance().log(message, "Slurm global sync hook", level);
}

void Hook::log(const boost::format& message, libsarus::LogLevel level) const {
    libsarus::Logger::getInstance().log(message.str(), "Slurm global sync hook", level);
}

}}} // namesapce
