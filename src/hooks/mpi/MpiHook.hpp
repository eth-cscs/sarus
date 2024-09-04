/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_mpi_MpiSupport_hpp
#define sarus_hooks_mpi_MpiSupport_hpp

#include <vector>
#include <unordered_map>

#include "AbiChecker.hpp"
#include "SharedLibrary.hpp"
#include "libsarus/LogLevel.hpp"
#include "libsarus/PathHash.hpp"
#include "libsarus/UserIdentity.hpp"

#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include "SharedLibrary.hpp"
#include "libsarus/Utility.hpp"

namespace sarus {
namespace hooks {
namespace mpi {

class MpiHook {
public:
    using HostToContainerLibsMap = std::unordered_map<boost::filesystem::path,
                                                      std::vector<SharedLibrary>,
                                                      libsarus::PathHash>;

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
                             const HostToContainerLibsMap& hostToContainerLibs,
                             const std::string& checkerType) const;
    void injectHostLibrary(const SharedLibrary& hostLib,
                           const HostToContainerLibsMap& hostToContainerLibs,
                           std::unique_ptr<AbiCompatibilityChecker> abiCompatibilityChecker) const;
    void performBindMounts() const;
    void createSymlinksInDynamicLinkerDefaultSearchDirs(const boost::filesystem::path& target,
                                                        const boost::filesystem::path& linkFilename,
                                                        const bool preserveRootLink) const;
    void log(const std::string& message, libsarus::LogLevel level) const;
    void log(const boost::format& message, libsarus::LogLevel level) const;

private:
    bool rootless = false;
    libsarus::hook::ContainerState containerState;
    boost::filesystem::path rootfsDir;
    libsarus::UserIdentity userIdentity;
    boost::filesystem::path ldconfig;
    std::vector<boost::filesystem::path> bindMounts;
    std::vector<SharedLibrary> containerLibs;
    std::vector<SharedLibrary> hostMpiLibs;
    std::vector<SharedLibrary> hostDepLibs;
    HostToContainerLibsMap hostToContainerMpiLibs;
    HostToContainerLibsMap hostToContainerDependencyLibs;
    bool containerHasIncompatibleLibraryVersion(const SharedLibrary& hostLib, const std::vector<SharedLibrary>& containerLibraries) const;
    std::string abiCompatibilityCheckerType{"major"};
};

}}} // namespace

#endif
