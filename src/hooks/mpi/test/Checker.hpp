/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_mpi_test_Checker_hpp
#define sarus_hooks_mpi_test_Checker_hpp

#include <fstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <unistd.h>
#include <sys/mount.h>

#include "hooks/common/Utility.hpp"
#include "hooks/mpi/MpiHook.hpp"
#include "test_utility/Misc.hpp"
#include "test_utility/config.hpp"
#include "test_utility/filesystem.hpp"
#include "test_utility/OCIHooks.hpp"

#include <CppUTest/TestHarness.h> // boost library must be included before CppUTest


namespace rj = rapidjson;

namespace sarus {
namespace hooks {
namespace mpi {
namespace test {

class Checker {
public:
    Checker& setEnvironmentVariablesInConfigJSON(const std::vector<std::string>& variables) {
        environmentVariables = variables;
        return *this;
    }

    Checker& setHostMpiLibraries(const std::vector<std::string>& libs) {
        for(const auto& lib : libs) {
            hostMpiLibs.push_back(bundleDir / lib);
        }
        return *this;
    }

    Checker& setHostMpiDependencyLibraries(const std::vector<std::string>& libs) {
        for(const auto& lib : libs) {
            hostMpiDependencyLibs.push_back(bundleDir / lib);
            containerMpiDependencyLibs.push_back(rootfsDir / "usr/lib" / lib);
        }
        return *this;
    }

    Checker& setContainerMpiLibraries(const std::vector<std::string>& libs) {
        for(const auto& lib : libs) {
            containerMpiLibs.push_back(rootfsDir / "usr/lib" / lib);
        }
        return *this;
    }

    Checker& setNonDefaultContainerMpiLibraries(const std::vector<std::string>& libs) {
        sarus::common::createFileIfNecessary(rootfsDir / "etc/ld.so.conf");
        std::ofstream linkerSearchConf((rootfsDir / "etc/ld.so.conf").c_str());
        for(const auto& lib : libs) {
            containerMpiLibs.push_back(rootfsDir / lib);
            linkerSearchConf << boost::filesystem::absolute(lib).parent_path().string() << std::endl;
        }
        expectSymlinks = true;
        return *this;
    }

    Checker& setMpiBindMounts(const std::vector<boost::filesystem::path>& bindMounts) {
        this->bindMounts = bindMounts;
        return *this;
    }

    Checker& setupDefaultLinkerDir(const boost::filesystem::path& dir) {
        sarus::common::createFoldersIfNecessary(rootfsDir / dir);
        defaultLinkerDir = dir;
        return *this;
    }

    void checkSuccessfull() const {
        setupTestEnvironment();
        MpiHook{}.activateMpiSupport();
        checkLibrariesAreInRootfs();
        checkConfigurationOfDynamicLinkerInContainer();
        checkBindMounts();
        checkSymlinks();
        cleanup();
    }

    void checkFailure() const {
        setupTestEnvironment();
        try {
            MpiHook{}.activateMpiSupport();
        }
        catch(...) {
            boost::filesystem::remove_all(bundleDir); // cleanup
            return;
        }
        boost::filesystem::remove_all(bundleDir); // cleanup
        CHECK(false); // expected failure/exception didn't occur
    }

private:
    void setupTestEnvironment() const {
        sarus::common::createFoldersIfNecessary(rootfsDir / "etc");
        createOCIBundleConfigJSON();
        createLibraries();
        sarus::common::executeCommand("ldconfig -r" + rootfsDir.string()); // run ldconfig in rootfs
        test_utility::ocihooks::writeContainerStateToStdin(bundleDir);
        sarus::common::setEnvironmentVariable("SARUS_MPI_LIBS=" + joinPaths(hostMpiLibs));
        sarus::common::setEnvironmentVariable("SARUS_MPI_DEPENDENCY_LIBS=" + joinPaths(hostMpiDependencyLibs));
        sarus::common::setEnvironmentVariable("SARUS_MPI_BIND_MOUNTS=" + joinPaths(bindMounts));
    }

    void createOCIBundleConfigJSON() const {
        auto doc = sarus::hooks::common::test::createBaseConfigJSON(rootfsDir, test_utility::misc::getNonRootUserIds());
        auto& allocator = doc.GetAllocator();
        for(const auto& v : environmentVariables) {
            doc["process"]["env"].PushBack(rj::Value{v.c_str(), allocator}, allocator);
        }

        try {
            sarus::common::writeJSON(doc, bundleDir / "config.json");
        }
        catch(const std::exception& e) {
            auto message = boost::format("Failed to write OCI Bundle's JSON configuration");
            SARUS_RETHROW_ERROR(e, message.str());
        }
    }

    void createLibraries() const {
        for(const auto& lib : hostMpiDependencyLibs) {
            sarus::common::createFoldersIfNecessary(lib.parent_path());
            boost::filesystem::copy_file(dummyHostLib, lib);
        }
        for(const auto& lib : hostMpiLibs) {
            sarus::common::createFoldersIfNecessary(lib.parent_path());
            boost::filesystem::copy_file(dummyHostLib, lib);
        }
        for(const auto& lib : containerMpiLibs) {
            sarus::common::createFoldersIfNecessary(lib.parent_path());
            boost::filesystem::copy_file(dummyContainerLib, lib);
        }
    }

    std::string joinPaths(const std::vector<boost::filesystem::path>& paths) const {
        auto s = std::string{};
        bool isFirstPath = true;
        for(const auto& path : paths) {
            if(isFirstPath) {
                isFirstPath = false;
            }
            else {
                s += ":";
            }
            s += path.string();
        }
        return s;
    }

    void checkLibrariesAreInRootfs() const {
        for(const auto& lib : containerMpiLibs) {
            CHECK(test_utility::filesystem::areFilesEqual(dummyHostLib, lib));
        }
        for(const auto& lib : containerMpiDependencyLibs) {
            CHECK(test_utility::filesystem::areFilesEqual(dummyHostLib, lib));
        }
    }

    void checkConfigurationOfDynamicLinkerInContainer() const {
        auto containerLibs = getContainerLibrariesFromDynamicLinker();

        auto expectedLibsInContainer = containerMpiLibs;
        expectedLibsInContainer.insert( expectedLibsInContainer.end(),
                                        containerMpiDependencyLibs.cbegin(),
                                        containerMpiDependencyLibs.cend());

        for(const auto& expectedLib : expectedLibsInContainer) {
            auto it = containerLibs.find(expectedLib.string());
            CHECK(it != containerLibs.cend());
        }
    }

    std::unordered_set<std::string> getContainerLibrariesFromDynamicLinker() const {
        auto libs = std::unordered_set<std::string>{};
        auto output = sarus::common::executeCommand("ldconfig -r" + rootfsDir.string() + " -p");
        std::stringstream stream{output};
        std::string line;
        std::getline(stream, line); // drop first line of output (header)
        while(std::getline(stream, line)) {
            auto pos = line.rfind(" => ");
            auto lib = (rootfsDir / line.substr(pos + 4)).string();
            libs.insert(std::move(lib));
        }
        return libs;
    }

    void checkBindMounts() const {
        for(const auto& mount : bindMounts) {
            CHECK(test_utility::filesystem::isSameBindMountedFile(mount, rootfsDir / mount));
        }
    }

    void checkSymlinks() const {
        if (!expectSymlinks) return;

        for(const auto& lib : containerMpiLibs) {
            auto libPath = boost::filesystem::absolute(boost::filesystem::relative(lib,rootfsDir));
            // Link with major, minor, patch numbers
            auto libName = lib.filename().string();
            CHECK(boost::filesystem::read_symlink(rootfsDir / defaultLinkerDir / libName) == libPath);
            // Link with major, minor numbers
            libName = libName.substr(0, libName.rfind("."));
            CHECK(boost::filesystem::read_symlink(rootfsDir / defaultLinkerDir / libName) == libPath);
            // Link with major number
            libName = libName.substr(0, libName.rfind("."));
            CHECK(boost::filesystem::read_symlink(rootfsDir / defaultLinkerDir / libName) == libPath);
            // Link without numbers
            libName = libName.substr(0, libName.rfind("."));
            CHECK(boost::filesystem::read_symlink(rootfsDir / defaultLinkerDir / libName) == libPath);
        }
    }

    void cleanup() const {
        for(const auto& lib : containerMpiLibs) {
            CHECK(umount(lib.c_str()) == 0);
        }
        for(const auto& lib : containerMpiDependencyLibs) {
            CHECK(umount(lib.c_str()) == 0);
        }
        for(const auto& mount : bindMounts) {
            CHECK(umount((rootfsDir / mount).c_str()) == 0);
        }
        boost::filesystem::remove_all(bundleDir);
    }

private:
    sarus::common::Config config = test_utility::config::makeConfig();
    boost::filesystem::path dummyHostLib = boost::filesystem::path{__FILE__}.parent_path() / "lib_dummy_host.so";
    boost::filesystem::path dummyContainerLib = boost::filesystem::path{__FILE__}.parent_path() / "lib_dummy_container.so";
    boost::filesystem::path bundleDir = boost::filesystem::path{ config.json.get()["OCIBundleDir"].GetString() };
    boost::filesystem::path rootfsDir = bundleDir / config.json.get()["rootfsFolder"].GetString();
    boost::filesystem::path defaultLinkerDir;

    std::vector<std::string> environmentVariables;
    std::vector<boost::filesystem::path> hostMpiLibs;
    std::vector<boost::filesystem::path> hostMpiDependencyLibs;
    std::vector<boost::filesystem::path> containerMpiLibs;
    std::vector<boost::filesystem::path> containerMpiDependencyLibs;
    std::vector<boost::filesystem::path> bindMounts;

    bool expectSymlinks = false;
};

}}}} // namespace

#endif
