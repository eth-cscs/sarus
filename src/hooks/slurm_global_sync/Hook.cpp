/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Hook.hpp"

#include <chrono>
#include <thread>
#include <iterator>
#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/Utility.hpp"
#include "hooks/common/Utility.hpp"

namespace sarus {
namespace hooks {
namespace slurm_global_sync {

Hook::Hook() {
    std::tie(bundleDir, pidOfContainer) = hooks::common::utility::parseStateOfContainerFromStdin();
    hooks::common::utility::enterNamespacesOfProcess(pidOfContainer);
    parseConfigJSONOfBundle();
    if(!isHookEnabled) {
        return;
    }

    boost::filesystem::path sarusInstallationPrefixDir = sarus::common::getEnvironmentVariable("SARUS_PREFIX_DIR");
    auto configFile =  sarusInstallationPrefixDir / "etc/sarus.json";
    auto configSchemaFile = sarusInstallationPrefixDir / "etc/sarus.schema.json";
    auto config = std::make_shared<sarus::common::Config>();
    config->initializeJson(config, configFile, configSchemaFile);
    config->userIdentity.uid = uidOfUser;
    config->userIdentity.gid = gidOfUser;
    localRepositoryDir = sarus::common::getLocalRepositoryDirectory(*config);

    syncDir = localRepositoryDir / "slurm_global_sync" / ("slurm-jobid-" + slurmJobID);
    syncDirArrival = syncDir / "arrival";
    syncFileArrival = syncDirArrival / ("slurm-procid-" + slurmProcID);
    syncDirDeparture = syncDir / "departure";
    syncFileDeparture = syncDirDeparture / ("slurm-procid-" + slurmProcID);
}

void Hook::performSynchronization() const {
    if(!isHookEnabled) {
        return;
    }

    signalArrival();
    while(!allInstancesArrived()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    signalDeparture();

    if(slurmProcID == "0") {
        while(!allInstancesDeparted()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        cleanupSyncDir();
    }
}

void Hook::signalArrival() const {
    std::cout << "hook creates sync file arrival: " << syncFileArrival << std::endl;
    createSyncFile(syncFileArrival);
}

void Hook::signalDeparture() const {
    createSyncFile(syncFileDeparture);
}

void Hook::cleanupSyncDir() const {
    boost::filesystem::remove_all(syncDir);
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

    // get uid + gid of user
    uidOfUser = json["process"]["user"]["uid"].GetInt();
    gidOfUser = json["process"]["user"]["gid"].GetInt();

    // get environment variables
    auto env = hooks::common::utility::parseEnvironmentVariablesFromOCIBundle(bundleDir);
    if(env.find("SLURM_JOB_ID") == env.cend()
        || env.find("SLURM_NTASKS") == env.cend()
        || env.find("SLURM_PROCID") == env.cend()) {
        isHookEnabled = false;
        return;
    }
    slurmJobID = env["SLURM_JOB_ID"];
    slurmNTasks = env["SLURM_NTASKS"];
    slurmProcID = env["SLURM_PROCID"];
}

}}} // namesapce
