/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "libsarus/Logger.hpp"
#include "libsarus/PathRAII.hpp"
#include "MountHookChecker.hpp"
#include "test_utility/unittest_main_function.hpp"


namespace sarus {
namespace hooks {
namespace mount {
namespace test {


TEST_GROUP(MountHookTestGroup) {
};

TEST(MountHookTestGroup, mounts) {
    MountHookChecker{}
        .setBindMounts({{"/var/opt", "/var/newopt"}})
        .setDeviceMounts({{"/dev/null", ":rw"}, {"/dev/zero", ":/dev/newzero:r"}})
        .checkSuccessful();

    MountHookChecker{}
        .setBindMounts({{"/non-existent", "/mnt/destination"}})
        .checkFailure();

    MountHookChecker{}
        .setBindMounts({{"/var/opt", "mnt/destination"}})
        .checkFailure();

    MountHookChecker{}
    .setDeviceMounts({{"/dev/non-existent", ":rw"}})
        .checkFailure();
}

void setupMockInDynamicLinkerCache(const boost::filesystem::path& rootfsDir, const boost::filesystem::path& mockPathInRootfs) {
    // create mock library file
    boost::filesystem::path dummyLib = boost::filesystem::path{__FILE__}
            .parent_path()
            .parent_path()
            .parent_path()
            .parent_path()
            .parent_path() / "CI/dummy_libs/lib_dummy_0.so";
    auto mockRealPath = rootfsDir / mockPathInRootfs;
    libsarus::filesystem::copyFile(dummyLib, mockRealPath);

    // populate ld.so.conf with mock directory
    libsarus::filesystem::createFileIfNecessary(rootfsDir / "etc/ld.so.conf");
    std::ofstream of{(rootfsDir / "etc/ld.so.conf").c_str()};
    of << mockPathInRootfs.parent_path().string() << "\n";
    of.close(); // write to disk

    // create /etc/ld.so.cache
    libsarus::process::executeCommand("ldconfig -r " + rootfsDir.string());
}

TEST(MountHookTestGroup, fi_provider_path_wildcard_replacement) {
    auto bundleDir = libsarus::PathRAII(
            libsarus::filesystem::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "mount-hook-test-bundle-dir"));
    auto rootfsDir = bundleDir.getPath() / "rootfs";
    auto bundleConfig = bundleDir.getPath() / "config.json";
    libsarus::filesystem::createFoldersIfNecessary(bundleDir.getPath());
    libsarus::filesystem::createFoldersIfNecessary(rootfsDir);

    auto config = test_utility::ocihooks::createBaseConfigJSON(rootfsDir, test_utility::misc::getNonRootUserIds());
    auto& allocator = config.GetAllocator();

    libsarus::environment::setVariable("LDCONFIG_PATH", "ldconfig");
    boost::filesystem::path libfabricContainerPath("/libfabricInstall/lib/libfabric.so.1");
    auto args = libsarus::CLIArguments{
                    "mount_hook",
                    "--mount=type=bind,src=/usr/lib64/libfabric/provider-fi.so,dst=<FI_PROVIDER_PATH>/provider-fi.so"};

    // FI_PROVIDER_PATH in environment
    {
        config["process"]["env"].PushBack("FI_PROVIDER_PATH=/fi/provider/path/envVar", allocator);
        libsarus::json::write(config, bundleConfig);

        test_utility::ocihooks::writeContainerStateToStdin(bundleDir.getPath());
        MountHook hook{args};
        CHECK(hook.getBindMounts().front()->getDestination() == boost::filesystem::path("/fi/provider/path/envVar/provider-fi.so"));
    }
    // FI_PROVIDER_PATH in environment and libfabric in dynamic linker cache
    {
        config["process"]["env"].SetArray();
        config["process"]["env"].PushBack("FI_PROVIDER_PATH=/fi/provider/path/envVar", allocator);
        libsarus::json::write(config, bundleConfig);

        setupMockInDynamicLinkerCache(rootfsDir, libfabricContainerPath);

        test_utility::ocihooks::writeContainerStateToStdin(bundleDir.getPath());
        MountHook hook{args};
        CHECK(hook.getBindMounts().front()->getDestination() == boost::filesystem::path("/fi/provider/path/envVar/provider-fi.so"));
    }
    // empty FI_PROVIDER_PATH in environment and libfabric in dynamic linker cache
    {
        config["process"]["env"].SetArray();
        config["process"]["env"].PushBack("FI_PROVIDER_PATH=", allocator);
        libsarus::json::write(config, bundleConfig);

        setupMockInDynamicLinkerCache(rootfsDir, libfabricContainerPath);

        test_utility::ocihooks::writeContainerStateToStdin(bundleDir.getPath());
        MountHook hook{args};
        CHECK(hook.getBindMounts().front()->getDestination() == (libfabricContainerPath.parent_path() / "libfabric/provider-fi.so"));
    }
    // libfabric only in dynamic linker cache
    {
        config["process"]["env"].SetArray();
        libsarus::json::write(config, bundleConfig);

        setupMockInDynamicLinkerCache(rootfsDir, libfabricContainerPath);

        test_utility::ocihooks::writeContainerStateToStdin(bundleDir.getPath());
        MountHook hook{args};
        CHECK(hook.getBindMounts().front()->getDestination() == (libfabricContainerPath.parent_path() / "libfabric/provider-fi.so"));
    }
    // no environment set and no ldconfig
    {
        config["process"]["env"].SetArray();
        libsarus::json::write(config, bundleConfig);

        CHECK(unsetenv("LDCONFIG_PATH") == 0);

        test_utility::ocihooks::writeContainerStateToStdin(bundleDir.getPath());
        MountHook hook{args};
        CHECK(hook.getBindMounts().front()->getDestination() == boost::filesystem::path("/usr/lib/provider-fi.so"));
    }
}

}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
