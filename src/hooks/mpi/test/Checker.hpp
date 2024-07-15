/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_mpi_test_Checker_hpp
#define sarus_hooks_mpi_test_Checker_hpp

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <unistd.h>
#include <sys/mount.h>

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
    ~Checker() {
        cleanup();
    }

    Checker& setHostMpiLibraries(const std::vector<boost::filesystem::path>& libs) {
        for(const auto& lib : libs) {
            hostMpiLibs.push_back(bundleDir / lib);
        }
        return *this;
    }

    Checker& setHostMpiDependencyLibraries(const std::vector<boost::filesystem::path>& libs) {
        for(const auto& lib : libs) {
            hostDependencyLibs.push_back(bundleDir / lib);
        }
        return *this;
    }

    Checker& setPreHookContainerLibraries(const std::vector<boost::filesystem::path>& libs) {
        preHookContainerLibs = libs;
        return *this;
    }

    Checker& expectPostHookContainerLibraries(const std::vector<boost::filesystem::path>& libs) {
        expectedPostHookContainerLibs = libs;
        return *this;
    }

    Checker& expectPreservedPostHookContainerLibraries(const std::vector<boost::filesystem::path>& libs) {
        preservedPostHookContainerLibs = libs;
        return *this;
    }

    Checker& setMpiBindMounts(const std::vector<boost::filesystem::path>& bindMounts) {
        this->bindMounts = bindMounts;
        return *this;
    }

    Checker& setExtraEnvironmentVariables(const std::unordered_map<std::string, std::string>& environmentVariables) {
        this->environmentVariables = environmentVariables;
        return *this;
    }

    void checkSuccessful() const {
        setupTestEnvironment();
        MpiHook{}.activateMpiSupport();
        if(expectedPostHookContainerLibs) {
            checkOnlyExpectedLibrariesAreInRootfs();
            checkExpectedLibrariesAreInLdSoCache();
            checkInjectedAndPreservedLibrariesAsExpected();
        }
        checkBindMounts();
        cleanup();
    }

    void checkFailure() const {
        setupTestEnvironment();
        try {
            MpiHook{}.activateMpiSupport();
        }
        catch(...) {
            return;
        }
        CHECK(false); // expected failure/exception didn't occur
    }

private:
    void setupTestEnvironment() const {
        libsarus::filesystem::createFoldersIfNecessary(rootfsDir / "etc");
        auto doc = test_utility::ocihooks::createBaseConfigJSON(rootfsDir, test_utility::misc::getNonRootUserIds());
        libsarus::json::write(doc, bundleDir / "config.json");
        createLibraries();
        setupDynamicLinkerInContainer();
        test_utility::ocihooks::writeContainerStateToStdin(bundleDir);
        libsarus::environment::setVariable("LDCONFIG_PATH", "ldconfig");
        libsarus::environment::setVariable("MPI_LIBS", libsarus::filesystem::makeColonSeparatedListOfPaths(hostMpiLibs));
        libsarus::environment::setVariable("MPI_DEPENDENCY_LIBS", libsarus::filesystem::makeColonSeparatedListOfPaths(hostDependencyLibs));
        libsarus::environment::setVariable("BIND_MOUNTS", libsarus::filesystem::makeColonSeparatedListOfPaths(bindMounts));
        for(auto& environmentVariable: environmentVariables) {
            libsarus::environment::setVariable(environmentVariable.first, environmentVariable.second);
        }
    }

    void createLibraries() const {
        for(const auto& lib : hostDependencyLibs) {
            libsarus::filesystem::copyFile(dummyHostLib, lib);
        }
        for(const auto& lib : hostMpiLibs) {
            libsarus::filesystem::copyFile(dummyHostLib, lib);
        }
        for(const auto& lib : preHookContainerLibs) {
            libsarus::filesystem::copyFile(dummyContainerLib, rootfsDir / lib);
        }
    }

    void setupDynamicLinkerInContainer() const {
        // write lib directories in /etc/ld.so.conf
        libsarus::filesystem::createFileIfNecessary(rootfsDir / "etc/ld.so.conf");
        std::ofstream of{(rootfsDir / "etc/ld.so.conf").c_str()};
        auto writtenSoFar = std::unordered_set<std::string>{};

        for(const auto& lib : preHookContainerLibs) {
            auto dir = lib.parent_path().string();
            if(writtenSoFar.find(dir) == writtenSoFar.cend()) {
                writtenSoFar.insert(dir);
                of << dir << "\n";
            }
        }
        of.close(); // write to disk

        // create /etc/ld.so.cache
        libsarus::process::executeCommand("ldconfig -r " + rootfsDir.string());
    }

    void checkOnlyExpectedLibrariesAreInRootfs() const {
        std::vector<std::string> expected, actual;
        for(auto& lib : *expectedPostHookContainerLibs) {
            expected.push_back((rootfsDir / lib).string());
        }
        auto begin = boost::filesystem::recursive_directory_iterator{rootfsDir};
        auto end = boost::filesystem::recursive_directory_iterator{};
        auto actual_p = std::vector<boost::filesystem::path>{};
        std::copy_if(begin, end, std::back_inserter(actual_p), libsarus::filesystem::isSharedLib);
        for(auto& entry: actual_p) {
            actual.push_back(entry.string());
        }

        std::sort(expected.begin(), expected.end());
        std::sort(actual.begin(), actual.end());

        CHECK_TRUE(std::equal(actual.begin(), actual.end(), expected.begin()));
    }

    void checkInjectedAndPreservedLibrariesAsExpected() const {
        for(const auto& lib : *expectedPostHookContainerLibs) {
            auto libReal = rootfsDir / libsarus::filesystem::realpathWithinRootfs(rootfsDir, lib);

            auto preservedLib = std::find(preservedPostHookContainerLibs.cbegin(), preservedPostHookContainerLibs.cend(), lib);
            if (preservedLib != preservedPostHookContainerLibs.cend()){
                CHECK(test_utility::filesystem::areFilesEqual(dummyContainerLib, libReal));
            }
            else{
                CHECK(test_utility::filesystem::areFilesEqual(dummyHostLib, libReal));
            }
        }
    }

    void checkExpectedLibrariesAreInLdSoCache() const {
        auto expecteds = std::unordered_set<std::string>{};
        for(const auto& lib : *expectedPostHookContainerLibs) {
            expecteds.insert(lib.filename().string());
        }

        auto actuals = std::unordered_set<std::string>{};
        for(const auto& lib : libsarus::sharedlibs::getListFromDynamicLinker("ldconfig", rootfsDir)) {
            actuals.insert(lib.filename().string());
        }

        for(const auto& expected : expecteds) {
            bool found = actuals.find(expected) != actuals.cend();
            CHECK(found);
        }
    }

    void checkBindMounts() const {
        for(const auto& mount : bindMounts) {
            CHECK(test_utility::filesystem::isSameBindMountedFile(mount, rootfsDir / mount));
        }
    }

    void cleanup() const {
        auto begin = boost::filesystem::recursive_directory_iterator{rootfsDir};
        auto end = boost::filesystem::recursive_directory_iterator{};
        for(auto it = begin; it!=end; ++it) {
            umount(it->path().c_str()); // attempt to unmount every file/folder in rootfs
        }
    }

private:
    test_utility::config::ConfigRAII configRAII = test_utility::config::makeConfig();
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
    boost::filesystem::path bundleDir = boost::filesystem::path{ configRAII.config->json["OCIBundleDir"].GetString() };
    boost::filesystem::path rootfsDir = bundleDir / configRAII.config->json["rootfsFolder"].GetString();

    std::vector<boost::filesystem::path> hostMpiLibs;
    std::vector<boost::filesystem::path> hostDependencyLibs;
    std::vector<boost::filesystem::path> preHookContainerLibs;
    boost::optional<std::vector<boost::filesystem::path>> expectedPostHookContainerLibs;
    std::vector<boost::filesystem::path> preservedPostHookContainerLibs;
    std::vector<boost::filesystem::path> bindMounts;
    std::unordered_map<std::string, std::string> environmentVariables;
};

}}}} // namespace

#endif
