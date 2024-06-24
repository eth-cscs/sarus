/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_SlurmGlobalSyncHook_hpp
#define sarus_hooks_SlurmGlobalSyncHook_hpp

#include <linux/types.h>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "libsarus/LogLevel.hpp"
#include "libsarus/Utility.hpp"

namespace sarus {
namespace hooks {
namespace slurm_global_sync {

class Hook {
public:
    Hook();
    void dropPrivileges() const;
    void loadConfigs();
    void performSynchronization() const;

    // these methods are public for test purpose
    void signalArrival() const;
    void signalDeparture() const;
    void cleanupSyncDir() const;

    bool allInstancesArrived() const;
    bool allInstancesDeparted() const;

private:
    void parseConfigJSONOfBundle();
    std::string getEnvironmentVariableInOCIBundleConfig(const rapidjson::Document& json,
                                                        const std::string& key) const;
    void synchronizeArrival() const;
    void synchronizeDeparture() const;
    void createSyncFile(const boost::filesystem::path& file) const;
    size_t countFilesInDirectory(const boost::filesystem::path& directory) const;
    void log(const std::string& message, libsarus::LogLevel level) const;
    void log(const boost::format& message, libsarus::LogLevel level) const;

private:
    bool isHookEnabled{ true };
    libsarus::hook::ContainerState containerState;
    boost::filesystem::path syncDir;
    boost::filesystem::path syncDirArrival;
    boost::filesystem::path syncDirDeparture;
    boost::filesystem::path syncFileArrival;
    boost::filesystem::path syncFileDeparture;
    uid_t uidOfUser;
    gid_t gidOfUser;
    std::string slurmJobID;
    std::string slurmStepID;
    std::string slurmNTasks;
    std::string slurmProcID;
};

}}} // namespace

#endif
