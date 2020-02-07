/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_mpi_MpiSupport_hpp
#define sarus_hooks_mpi_MpiSupport_hpp

#include <vector>
#include <unordered_map>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <sys/types.h>

#include "common/LogLevel.hpp"
#include "common/PathHash.hpp"

namespace sarus {
namespace hooks {
namespace mpi {

class MpiHook {
public:
    using HostToContainerLibsMap = std::unordered_map<boost::filesystem::path,
                                                      std::vector<boost::filesystem::path>,
                                                      sarus::common::PathHash>;

public:
    MpiHook();
    void activateMpiSupport();

private:
    void parseConfigJSONOfBundle();
    void parseEnvironmentVariables();
    HostToContainerLibsMap mapHostToContainerLibraries(const std::vector<boost::filesystem::path>& hostLibs,
                                                       const std::vector<boost::filesystem::path>& containerLibs) const;
    void checkHostMpiLibrariesHaveAbiVersion() const;
    void checkContainerMpiLibrariesHaveAbiVersion() const;
    void checkHostContainerAbiCompatibility(const HostToContainerLibsMap& hostToContainerLibs) const;
    void injectHostLibraries(const std::vector<boost::filesystem::path>& hostLibs,
                             const HostToContainerLibsMap& hostToContainerLibs) const;
    void injectHostLibrary(const boost::filesystem::path& hostLib,
                           const HostToContainerLibsMap& hostToContainerLibs) const;
    void performBindMounts() const;
    bool injectHostLibrary(
        const boost::filesystem::path& hostLib,
        const std::function<bool(const boost::filesystem::path&, const boost::filesystem::path&)>& abiCompatibilityCheck) const;
    void createSymlinksInDynamicLinkerDefaultSearchDirs(const boost::filesystem::path& target,
                                                        const boost::filesystem::path& linkFilename) const;
    void log(const std::string& message, sarus::common::LogLevel level) const;
    void log(const boost::format& message, sarus::common::LogLevel level) const;

private:
    bool isHookEnabled{ false };
    boost::filesystem::path bundleDir;
    boost::filesystem::path rootfsDir;
    pid_t pidOfContainer;
    boost::filesystem::path ldconfig;
    std::vector<boost::filesystem::path> hostMpiLibs;
    std::vector<boost::filesystem::path> hostDependencyLibs;
    std::vector<boost::filesystem::path> bindMounts;
    std::vector<boost::filesystem::path> containerLibraries;
    HostToContainerLibsMap hostToContainerMpiLibs;
    HostToContainerLibsMap hostToContainerDependencyLibs;
};

}}} // namespace

#endif
