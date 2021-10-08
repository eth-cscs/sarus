/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string>


#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>

#include "test_utility/config.hpp"
#include "test_utility/filesystem.hpp"
#include "runtime/DeviceMount.hpp"
#include "common/DeviceAccess.hpp"
#include "common/PathRAII.hpp"
#include "common/Utility.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace runtime {
namespace test{

TEST_GROUP(DeviceMountTestGroup) {
};

#ifdef NOTROOT
IGNORE_TEST(DeviceMountTestGroup, constructor) {
#else
TEST(DeviceMountTestGroup, constructor) {
#endif
    auto testDir = sarus::common::PathRAII(
        sarus::common::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "deviceMount-test-constructor"));

    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    size_t mount_flags = 0;
    auto devAccess = common::DeviceAccess("rwm");

    // regular usage
    {
        // Create source file as a character device file with 666 file mode
        auto testDeviceFile = testDir.getPath() / "testDevice";
        auto fileMode = S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
        auto deviceID = makedev(511, 511);
        if (mknod(testDeviceFile.c_str(), fileMode, deviceID) != 0) {
            auto message = boost::format("Failed to mknod test device file: %s") % strerror(errno);
            FAIL(message.str().c_str());
        }

        DeviceMount(testDeviceFile, testDeviceFile, mount_flags, devAccess, config);
    }
    // source path is not a device file
    {
        auto noDeviceFile = testDir.getPath() / "notADevice";
        sarus::common::createFileIfNecessary(noDeviceFile);

        CHECK_THROWS(common::Error, DeviceMount(noDeviceFile, noDeviceFile, mount_flags, devAccess, config));
    }
}

#ifdef NOTROOT
IGNORE_TEST(DeviceMountTestGroup, getters) {
#else
TEST(DeviceMountTestGroup, getters) {
#endif
    auto testDir = sarus::common::PathRAII(
        sarus::common::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "deviceMount-test-getters"));

    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    size_t mount_flags = 0;

    {
        // Create source file as a character device file with 666 file mode
        auto testDeviceFile = testDir.getPath() / "testDevice0";
        auto fileMode = S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
        auto deviceID = makedev(511, 511);
        if (mknod(testDeviceFile.c_str(), fileMode, deviceID) != 0) {
            auto message = boost::format("Failed to mknod test device file: %s") % strerror(errno);
            FAIL(message.str().c_str());
        }

        auto devAccess = common::DeviceAccess("rwm");

        auto devMount = DeviceMount(testDeviceFile, testDeviceFile, mount_flags, devAccess, config);
        CHECK(devMount.getType() == 'c');
        CHECK(devMount.getMajorID() == 511);
        CHECK(devMount.getMinorID() == 511);
        CHECK(devMount.getAccess().string() == std::string{"rwm"});
    }
    {
        // Create source file as a block device file with 666 file mode
        auto testDeviceFile = testDir.getPath() / "testDevice0";
        auto fileMode = S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
        auto deviceID = makedev(477, 488);
        if (mknod(testDeviceFile.c_str(), fileMode, deviceID) != 0) {
            auto message = boost::format("Failed to mknod test device file: %s") % strerror(errno);
            FAIL(message.str().c_str());
        }

        auto devAccess = common::DeviceAccess("rw");

        auto devMount = DeviceMount(testDeviceFile, testDeviceFile, mount_flags, devAccess, config);
        CHECK(devMount.getType() == 'b');
        CHECK(devMount.getMajorID() == 477);
        CHECK(devMount.getMinorID() == 488);
        CHECK(devMount.getAccess().string() == std::string{"rw"});
    }
}

#ifdef NOTROOT
IGNORE_TEST(DeviceMountTestGroup, performMount) {
#else
TEST(DeviceMountTestGroup, performMount) {
#endif
    auto testDir = sarus::common::PathRAII(
        sarus::common::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "deviceMount-test-performMount"));

    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    auto bundleDirRAII = common::PathRAII{boost::filesystem::path{config->json["OCIBundleDir"].GetString()}};
    const auto& bundleDir = bundleDirRAII.getPath();
    auto rootfsDir = bundleDir / boost::filesystem::path{config->json["rootfsFolder"].GetString()};
    common::createFoldersIfNecessary(rootfsDir);
    auto overlayfsLowerDir = bundleDir / "overlay/rootfs-lower"; // hardcoded so in production code being tested
    common::createFoldersIfNecessary(overlayfsLowerDir);

    auto sourceFile = testDir.getPath() / "testDevice0";
    auto destinationFile = boost::filesystem::path{"/dev/testDevice0"};

    // Create source file as a character device file with 666 file mode
    auto fileMode = S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    auto deviceID = makedev(511, 511);
    if (mknod(sourceFile.c_str(), fileMode, deviceID) != 0) {
        auto message = boost::format("Failed to mknod test device file: %s") % strerror(errno);
        FAIL(message.str().c_str());
    }

    size_t mount_flags = 0;
    auto devAccess = common::DeviceAccess("rwm");

    // perform the mount
    runtime::DeviceMount{sourceFile, destinationFile, mount_flags, devAccess, config}.performMount();
    CHECK(test_utility::filesystem::isSameBindMountedFile(sourceFile, rootfsDir / destinationFile));
    CHECK(common::getDeviceID(rootfsDir / destinationFile) == deviceID);
    CHECK(common::getDeviceType(rootfsDir / destinationFile) == 'c');

    // cleanup
    CHECK(umount((rootfsDir / destinationFile).c_str()) == 0);
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
