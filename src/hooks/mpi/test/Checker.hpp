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

    Checker& setCustomMountParentDir(const std::string& path) {
        customMountPath = path;
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
        sarus::common::createFoldersIfNecessary(rootfsDir / "etc");
        auto doc = test_utility::ocihooks::createBaseConfigJSON(rootfsDir, test_utility::misc::getNonRootUserIds());
        if(!customMountPath.empty()) {
            rapidjson::Value jCustomMountPath(rapidjson::kObjectType);
            jCustomMountPath.AddMember(
                "com.hooks.mpi.mount_dir_parent", 
                rj::Value{customMountPath.c_str(), doc.GetAllocator()},
                doc.GetAllocator()
            );
            if(!doc.HasMember("annotations")) {
                doc.AddMember("annotations", jCustomMountPath, doc.GetAllocator());
            } else {
                doc["annotations"] = jCustomMountPath;
            }
        }
        sarus::common::writeJSON(doc, bundleDir / "config.json");
        createLibraries();
        setupDynamicLinkerInContainer();
        test_utility::ocihooks::writeContainerStateToStdin(bundleDir);
        sarus::common::setEnvironmentVariable("LDCONFIG_PATH", "ldconfig");
        sarus::common::setEnvironmentVariable("MPI_LIBS", sarus::common::makeColonSeparatedListOfPaths(hostMpiLibs));
        sarus::common::setEnvironmentVariable("MPI_DEPENDENCY_LIBS", sarus::common::makeColonSeparatedListOfPaths(hostDependencyLibs));
        sarus::common::setEnvironmentVariable("BIND_MOUNTS", sarus::common::makeColonSeparatedListOfPaths(bindMounts));
    }

    void createLibraries() const {
        for(const auto& lib : hostDependencyLibs) {
            sarus::common::copyFile(dummyHostLib, lib);
        }
        for(const auto& lib : hostMpiLibs) {
            sarus::common::copyFile(dummyHostLib, lib);
        }
        for(const auto& lib : preHookContainerLibs) {
            sarus::common::copyFile(dummyContainerLib, rootfsDir / lib);
        }
    }

    void setupDynamicLinkerInContainer() const {
        // write lib directories in /etc/ld.so.conf
        sarus::common::createFileIfNecessary(rootfsDir / "etc/ld.so.conf");
        std::ofstream of{(rootfsDir / "etc/ld.so.conf").c_str()};
        auto writtenSoFar = std::unordered_set<std::string>{};

        for(const auto& lib : preHookContainerLibs) {
            auto dir = lib.parent_path().string();
            if(writtenSoFar.find(dir) == writtenSoFar.cend()) {
                writtenSoFar.insert(dir);
                of << dir << "\n";
            }
        }
        of << "include /etc/ld.so.conf.d/*.conf" << std::endl;
        of.close(); // write to disk

        // create /etc/ld.so.cache
        sarus::common::executeCommand("ldconfig -r " + rootfsDir.string());
    }

    void checkOnlyExpectedLibrariesAreInRootfs() const {
        auto expected = *expectedPostHookContainerLibs;
        std::sort(expected.begin(), expected.end(), [](boost::filesystem::path first, boost::filesystem::path second) {
            return first.filename().string() > second.filename().string();
        });

        auto begin = boost::filesystem::recursive_directory_iterator{rootfsDir};
        auto end = boost::filesystem::recursive_directory_iterator{};
        auto actual = std::vector<boost::filesystem::path>{};
        std::copy_if(begin, end, std::back_inserter(actual), sarus::common::isSharedLib);
        std::sort(actual.begin(), actual.end(), [](boost::filesystem::path first, boost::filesystem::path second) {
            return first.filename().string() > second.filename().string();
        });

        CHECK_EQUAL(actual.size(), expected.size());
        for(auto itra{actual.begin()}, itre{expected.begin()}; itra != actual.cend() && itre != expected.cend(); ++itra, ++itre) {
            CHECK_EQUAL(itra->filename().string(), itre->filename().string());
        }
    }

    void checkInjectedAndPreservedLibrariesAsExpected() const {
        for(const auto& lib : *expectedPostHookContainerLibs) {
            auto libReal = rootfsDir / sarus::common::realpathWithinRootfs(rootfsDir, lib);

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
        for(const auto& lib : sarus::common::getSharedLibsFromDynamicLinker("ldconfig", rootfsDir)) {
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
    std::string customMountPath;
};

}}}} // namespace

#endif
