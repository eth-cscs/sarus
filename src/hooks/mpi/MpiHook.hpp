/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_mpi_MpiSupport_hpp
#define sarus_hooks_mpi_MpiSupport_hpp

#include <unordered_map>
#include <boost/filesystem.hpp>


namespace sarus {
namespace hooks {
namespace mpi {

class MpiHook {
public:
    void activateMpiSupport();

private:
    void parseConfigJSONOfBundle();
    void parseEnvironmentVariables();
    std::vector<boost::filesystem::path> parseListOfPaths(const std::string& s);
    void getContainerLibrariesFromDynamicLinker();
    void replaceMpiLibrariesInContainer() const;
    void mountDependencyLibrariesIntoContainer() const;
    void performBindMounts() const;
    bool areLibrariesABICompatible(const boost::filesystem::path& hostLib, const boost::filesystem::path& containerLib) const;
    bool isLibraryInDefaultLinkerDirectory(const boost::filesystem::path& lib) const;
    void createSymlinksInDefaultLinkerDirectory(const boost::filesystem::path& lib) const;
    std::tuple<int, int, int> getVersionNumbersOfLibrary(const boost::filesystem::path& lib) const;
    std::string getStrippedLibraryName(const boost::filesystem::path& path) const;

private:
    bool isHookEnabled{ false };
    boost::filesystem::path bundleDir;
    boost::filesystem::path rootfsDir;
    pid_t pidOfContainer;
    std::vector<boost::filesystem::path> hostMpiLibs;
    std::vector<boost::filesystem::path> hostMpiDependencyLibs;
    std::vector<boost::filesystem::path> bindMounts;
    std::unordered_map<std::string, boost::filesystem::path> containerLibraries;
};

}}} // namespace

#endif
