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
#include "SharedLibrary.hpp"

namespace sarus {
namespace hooks {
namespace mpi {

class MpiHook {
public:
    using HostToContainerLibsMap = std::unordered_map<boost::filesystem::path,
                                                      std::vector<SharedLibrary>,
                                                      sarus::common::PathHash>;

public:
    MpiHook();
    void activateMpiSupport();

private:
    void parseConfigJSONOfBundle();
    void parseEnvironmentVariables();
    HostToContainerLibsMap mapHostTocontainerLibs(const std::vector<SharedLibrary>& hostLibs,
                                                  const std::vector<SharedLibrary>& containerLibs) const;
    void checkHostMpiLibrariesHaveAbiVersion() const;
    void checkContainerMpiLibrariesHaveAbiVersion() const;
    void checkHostContainerAbiCompatibility(const HostToContainerLibsMap& hostToContainerLibs) const;
    void injectHostLibraries(const std::vector<SharedLibrary>& hostLibs,
                             const HostToContainerLibsMap& hostToContainerLibs) const;
    void injectHostLibrary(const SharedLibrary& hostLib,
                           const HostToContainerLibsMap& hostToContainerLibs) const;
    void performBindMounts() const;
    bool injectHostLibrary(
        const boost::filesystem::path& hostLib,
        const std::function<bool(const boost::filesystem::path&, const boost::filesystem::path&)>& abiCompatibilityCheck) const;
    void createSymlinksInDynamicLinkerDefaultSearchDirs(const boost::filesystem::path& target,
                                                        const boost::filesystem::path& linkFilename,
                                                        const bool preserveRootLink) const;
    void log(const std::string& message, sarus::common::LogLevel level) const;
    void log(const boost::format& message, sarus::common::LogLevel level) const;

private:
    boost::filesystem::path bundleDir;
    boost::filesystem::path rootfsDir;
    pid_t pidOfContainer;
    boost::filesystem::path ldconfig;
    std::vector<boost::filesystem::path> bindMounts;
    std::vector<SharedLibrary> containerLibs;
    std::vector<SharedLibrary> hostMpiLibs;
    std::vector<SharedLibrary> hostDepLibs;
    HostToContainerLibsMap hostToContainerMpiLibs;
    HostToContainerLibsMap hostToContainerDependencyLibs;
    bool containerHasIncompatibleLibraryVersion(const SharedLibrary& hostLib, const std::vector<SharedLibrary>& containerLibraries) const;
};

}}} // namespace

#endif
