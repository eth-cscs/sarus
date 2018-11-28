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

    localRepositoryBaseDir = sarus::common::getEnvironmentVariable("SARUS_LOCAL_REPOSITORY_BASE_DIR");
    localRepositoryDir = sarus::common::getLocalRepositoryDirectory(localRepositoryBaseDir, uidOfUser);

    syncDir = localRepositoryDir / "slurm_global_sync" / ("slurm-jobid-" + slurmJobID);
    syncDirArrival = syncDir / "arrival";
    syncFileArrival = syncDirArrival / ("slurm-procid-" + slurmProcID);
    syncDirDeparture = syncDir / "departure";
    syncFileDeparture = syncDirDeparture / ("slurm-procid-" + slurmProcID);
}

void Hook::performSynchronization() const {
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
    slurmJobID = getEnvironmentVariableInOCIBundleConfig(json, "SLURM_JOB_ID");
    slurmNTasks = getEnvironmentVariableInOCIBundleConfig(json, "SLURM_NTASKS");
    slurmProcID = getEnvironmentVariableInOCIBundleConfig(json, "SLURM_PROCID");
}

std::string Hook::getEnvironmentVariableInOCIBundleConfig(const rapidjson::Document& json, const std::string& key) const {
    for(const auto& variable : json["process"]["env"].GetArray()) {
        std::string k, v;
        std::tie(k, v) = sarus::common::parseEnvironmentVariable(variable.GetString());
        if(k == key) {
            return v;
        }
    }
    auto message = boost::format("environment of OCI bundle's config.json doens't contain variable with key %s") % key;
    SARUS_THROW_ERROR(message.str());
}

}}} // namesapce
