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
        sarus::common::setEnvironmentVariable("SARUS_MPI_LIBS=" + sarus::common::makeColonSeparatedListOfPaths(hostMpiLibs));
        sarus::common::setEnvironmentVariable("SARUS_MPI_DEPENDENCY_LIBS=" + sarus::common::makeColonSeparatedListOfPaths(hostMpiDependencyLibs));
        sarus::common::setEnvironmentVariable("SARUS_MPI_BIND_MOUNTS=" + sarus::common::makeColonSeparatedListOfPaths(bindMounts));
    }

    void createOCIBundleConfigJSON() const {
        auto doc = test_utility::ocihooks::createBaseConfigJSON(rootfsDir, test_utility::misc::getNonRootUserIds());
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

    void checkLibrariesAreInRootfs() const {
        for(const auto& lib : containerMpiLibs) {
            CHECK(test_utility::filesystem::areFilesEqual(dummyHostLib, lib));
        }
        for(const auto& lib : containerMpiDependencyLibs) {
            CHECK(test_utility::filesystem::areFilesEqual(dummyHostLib, lib));
        }
    }

    void checkConfigurationOfDynamicLinkerInContainer() const {
        auto containerLibs = sarus::common::getLibrariesFromDynamicLinker("ldconfig", rootfsDir);

        auto expectedLibsInContainer = containerMpiLibs;
        expectedLibsInContainer.insert( expectedLibsInContainer.end(),
                                        containerMpiDependencyLibs.cbegin(),
                                        containerMpiDependencyLibs.cend());

        for(const auto& expectedLib : expectedLibsInContainer) {
            auto hasFilenameOfExpectedLib = [&expectedLib](const boost::filesystem::path& lib) {
                return lib.filename() == expectedLib.filename();
            };
            auto it = std::find_if(containerLibs.cbegin(), containerLibs.cend(), hasFilenameOfExpectedLib);
            CHECK(it != containerLibs.cend());
        }
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
            for (const auto& linkerDir : defaultLinkerDirs) {
                // Link with major, minor, patch numbers
                auto libName = lib.filename().string();
                CHECK(boost::filesystem::read_symlink(rootfsDir / linkerDir / libName) == libPath);
                // Link with major, minor numbers
                libName = libName.substr(0, libName.rfind("."));
                CHECK(boost::filesystem::read_symlink(rootfsDir / linkerDir / libName) == libPath);
                // Link with major number
                libName = libName.substr(0, libName.rfind("."));
                CHECK(boost::filesystem::read_symlink(rootfsDir / linkerDir / libName) == libPath);
                // Link without numbers
                libName = libName.substr(0, libName.rfind("."));
                CHECK(boost::filesystem::read_symlink(rootfsDir / linkerDir / libName) == libPath);
            }
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
    boost::filesystem::path dummyHostLib = boost::filesystem::path{__FILE__}
        .parent_path()
        .parent_path()
        .parent_path()
        .parent_path()
        .parent_path() / "CI/dummy_libs/lib_dummy_0.so";
    boost::filesystem::path dummyContainerLib = boost::filesystem::path{__FILE__}
        .parent_path()
        .parent_path()
        .parent_path()
        .parent_path()
        .parent_path() / "CI/dummy_libs/lib_dummy_1.so";
    boost::filesystem::path bundleDir = boost::filesystem::path{ config.json["OCIBundleDir"].GetString() };
    boost::filesystem::path rootfsDir = bundleDir / config.json["rootfsFolder"].GetString();

    std::vector<std::string> environmentVariables;
    std::vector<boost::filesystem::path> hostMpiLibs;
    std::vector<boost::filesystem::path> hostMpiDependencyLibs;
    std::vector<boost::filesystem::path> containerMpiLibs;
    std::vector<boost::filesystem::path> containerMpiDependencyLibs;
    std::vector<boost::filesystem::path> bindMounts;
    std::vector<boost::filesystem::path> defaultLinkerDirs{"lib64", "lib"};

    bool expectSymlinks = false;
};

}}}} // namespace

#endif
