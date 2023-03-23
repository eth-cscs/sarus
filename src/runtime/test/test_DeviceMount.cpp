/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string>

#include <sys/mount.h>
#include <sys/sysmacros.h>

#include "test_utility/config.hpp"
#include "test_utility/filesystem.hpp"
#include "runtime/DeviceMount.hpp"
#include "common/DeviceAccess.hpp"
#include "common/PathRAII.hpp"
#include "common/Utility.hpp"
#include "test_utility/filesystem.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace runtime {
namespace test{

TEST_GROUP(DeviceMountTestGroup) {
};

#ifdef ASROOT
TEST(DeviceMountTestGroup, constructor) {
#else
IGNORE_TEST(DeviceMountTestGroup, constructor) {
#endif
    auto testDir = sarus::common::PathRAII(
        sarus::common::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "deviceMount-test-constructor"));
    common::createFoldersIfNecessary(testDir.getPath());

    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    size_t mount_flags = 0;
    auto devAccess = common::DeviceAccess("rwm");

    // regular usage
    {
        auto testDeviceFile = testDir.getPath() / "testDevice";
        auto majorID = 511u;
        auto minorID = 511u;
        test_utility::filesystem::createCharacterDeviceFile(testDeviceFile, majorID, minorID);
        auto mountObject = Mount{testDeviceFile, testDeviceFile, mount_flags, config};

        DeviceMount(std::move(mountObject), devAccess);
    }
    // source path is not a device file
    {
        auto noDeviceFile = testDir.getPath() / "notADevice";
        sarus::common::createFileIfNecessary(noDeviceFile);
        auto mountObject = Mount{noDeviceFile, noDeviceFile, mount_flags, config};

        CHECK_THROWS(common::Error, DeviceMount(std::move(mountObject), devAccess));
    }
}

#ifdef ASROOT
TEST(DeviceMountTestGroup, getters) {
#else
IGNORE_TEST(DeviceMountTestGroup, getters) {
#endif
    auto testDir = sarus::common::PathRAII(
        sarus::common::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "deviceMount-test-getters"));
    common::createFoldersIfNecessary(testDir.getPath());

    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    size_t mount_flags = 0;

    {
        // Create source file as a character device file with 666 file mode
        auto testDeviceFile = testDir.getPath() / "sarusTestDevice0";
        auto majorID = 511u;
        auto minorID = 511u;
        test_utility::filesystem::createCharacterDeviceFile(testDeviceFile, majorID, minorID);

        auto mountObject = Mount{testDeviceFile, testDeviceFile, mount_flags, config};
        auto devAccess = common::DeviceAccess("rwm");

        auto devMount = DeviceMount(std::move(mountObject), devAccess);
        CHECK(devMount.getType() == 'c');
        CHECK(devMount.getMajorID() == majorID);
        CHECK(devMount.getMinorID() == minorID);
        CHECK(devMount.getAccess().string() == std::string{"rwm"});

        boost::filesystem::remove(testDeviceFile);
    }
    {
        auto testDeviceFile = testDir.getPath() / "sarusTestDevice1";
        auto majorID = 477u;
        auto minorID = 488u;
        test_utility::filesystem::createBlockDeviceFile(testDeviceFile, majorID, minorID);

        auto mountObject = Mount{testDeviceFile, testDeviceFile, mount_flags, config};
        auto devAccess = common::DeviceAccess("rw");

        auto devMount = DeviceMount(std::move(mountObject), devAccess);
        CHECK(devMount.getType() == 'b');
        CHECK_EQUAL(devMount.getMajorID(),  majorID);
        CHECK(devMount.getMinorID() == minorID);
        CHECK(devMount.getAccess().string() == std::string{"rw"});

        boost::filesystem::remove(testDeviceFile);
    }
}

#ifdef ASROOT
TEST(DeviceMountTestGroup, performMount) {
#else
IGNORE_TEST(DeviceMountTestGroup, performMount) {
#endif
    auto testDir = sarus::common::PathRAII(
        sarus::common::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "deviceMount-test-performMount"));
    common::createFoldersIfNecessary(testDir.getPath());

    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    auto bundleDirRAII = common::PathRAII{boost::filesystem::path{config->json["OCIBundleDir"].GetString()}};
    const auto& bundleDir = bundleDirRAII.getPath();
    auto rootfsDir = bundleDir / boost::filesystem::path{config->json["rootfsFolder"].GetString()};
    common::createFoldersIfNecessary(rootfsDir);

    auto sourceFile = testDir.getPath() / "sarusTestDevice0";
    auto destinationFile = boost::filesystem::path{"/dev/sarusTestDevice0"};

    auto majorID = 511u;
    auto minorID = 511u;
    test_utility::filesystem::createCharacterDeviceFile(sourceFile, majorID, minorID);

    size_t mount_flags = 0;
    auto mountObject = Mount{sourceFile, destinationFile, mount_flags, config};
    auto devAccess = common::DeviceAccess("rwm");

    // perform the mount
    runtime::DeviceMount{std::move(mountObject), devAccess}.performMount();
    CHECK(test_utility::filesystem::isSameBindMountedFile(sourceFile, rootfsDir / destinationFile));
    CHECK(common::getDeviceID(rootfsDir / destinationFile) == makedev(majorID, minorID));
    CHECK(common::getDeviceType(rootfsDir / destinationFile) == 'c');

    // cleanup
    CHECK(umount((rootfsDir / destinationFile).c_str()) == 0);
    boost::filesystem::remove(sourceFile);
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
