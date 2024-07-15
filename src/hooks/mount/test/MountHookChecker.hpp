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
#include <unordered_map>
#include <unistd.h>
#include <sys/mount.h>

#include "hooks/mount/MountHook.hpp"
#include "test_utility/Misc.hpp"
#include "test_utility/config.hpp"
#include "test_utility/filesystem.hpp"
#include "test_utility/OCIHooks.hpp"

#include <CppUTest/TestHarness.h> // boost library must be included before CppUTest


namespace rj = rapidjson;

namespace sarus {
namespace hooks {
namespace mount {
namespace test {

class MountHookChecker {
public:
    ~MountHookChecker() {
        cleanup();
    }

    MountHookChecker& setBindMounts(const std::unordered_map<std::string, std::string>& bindMounts) {
        this->bindMounts = bindMounts;
        return *this;
    }

    MountHookChecker& setDeviceMounts(const std::unordered_map<std::string, std::string>& deviceMounts) {
        this->deviceMounts = deviceMounts;
        return *this;
    }

    void checkSuccessful() const {
        setupTestEnvironment();
        auto args = generateCliArgs();
        MountHook{args}.activate();
        checkBindMounts();
        cleanup();
    }

    void checkFailure() const {
        setupTestEnvironment();
        auto args = generateCliArgs();
        try {
            MountHook{args}.activate();
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
        test_utility::ocihooks::writeContainerStateToStdin(bundleDir);
    }

    libsarus::CLIArguments generateCliArgs() const {
        auto args = libsarus::CLIArguments{"mount_hook"};
        for (const auto & mount : bindMounts) {
            auto argFormat = boost::format("--mount=type=bind,src=%s,dst=%s") % mount.first % mount.second;
            args.push_back(argFormat.str());
        }
        for (const auto & mount : deviceMounts) {
            auto argFormat = boost::format("--device=%s%s") % mount.first % mount.second;
            args.push_back(argFormat.str());
        }
        return args;
    }

    void checkBindMounts() const {
        for(const auto& mount : bindMounts) {
            if (mount.second.empty()) {
                CHECK(test_utility::filesystem::isSameBindMountedFile(mount.first, rootfsDir / mount.first));
            }
            else {
                CHECK(test_utility::filesystem::isSameBindMountedFile(mount.first, rootfsDir / mount.second));
            }
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
    boost::filesystem::path bundleDir = boost::filesystem::path{ configRAII.config->json["OCIBundleDir"].GetString() };
    boost::filesystem::path rootfsDir = bundleDir / configRAII.config->json["rootfsFolder"].GetString();

    std::unordered_map<std::string, std::string> bindMounts;
    std::unordered_map<std::string, std::string> deviceMounts;
};

}}}} // namespace

#endif
