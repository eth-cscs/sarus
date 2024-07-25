/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string>
#include <sys/mount.h>
#include <sys/sysmacros.h>

#include "aux/filesystem.hpp"
#include "aux/unitTestMain.hpp"
#include "libsarus/DeviceAccess.hpp"
#include "libsarus/DeviceMount.hpp"
#include "libsarus/PathRAII.hpp"
#include "libsarus/Utility.hpp"


namespace libsarus {
namespace test {

TEST_GROUP(DeviceMountTestGroup) {
};

#ifdef ASROOT
TEST(DeviceMountTestGroup, constructor) {
#else
IGNORE_TEST(DeviceMountTestGroup, constructor) {
#endif
    auto testDir = libsarus::PathRAII(
        libsarus::filesystem::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "deviceMount-test-constructor"));
    libsarus::filesystem::createFoldersIfNecessary(testDir.getPath());

    auto bundleDirRAII = libsarus::PathRAII{libsarus::filesystem::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("test-bundle-dir"))};
    const auto& rootfsDir = bundleDirRAII.getPath() / "rootfs";
    libsarus::UserIdentity userIdentity;

    size_t mount_flags = 0;
    auto devAccess = libsarus::DeviceAccess("rwm");

    // regular usage
    {
        auto testDeviceFile = testDir.getPath() / "testDevice";
        auto majorID = 511u;
        auto minorID = 511u;
        aux::filesystem::createCharacterDeviceFile(testDeviceFile, majorID, minorID);
        auto mountObject = libsarus::Mount{testDeviceFile, testDeviceFile, mount_flags, rootfsDir, userIdentity};

        DeviceMount(std::move(mountObject), devAccess);
    }
    // source path is not a device file
    {
        auto noDeviceFile = testDir.getPath() / "notADevice";
        libsarus::filesystem::createFileIfNecessary(noDeviceFile);
        auto mountObject = libsarus::Mount{noDeviceFile, noDeviceFile, mount_flags, rootfsDir, userIdentity};

        CHECK_THROWS(libsarus::Error, DeviceMount(std::move(mountObject), devAccess));
    }
}

#ifdef ASROOT
TEST(DeviceMountTestGroup, getters) {
#else
IGNORE_TEST(DeviceMountTestGroup, getters) {
#endif
    auto testDir = libsarus::PathRAII(
        libsarus::filesystem::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "deviceMount-test-getters"));
    libsarus::filesystem::createFoldersIfNecessary(testDir.getPath());

    auto bundleDirRAII = libsarus::PathRAII{libsarus::filesystem::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("test-bundle-dir"))};
    const auto& rootfsDir = bundleDirRAII.getPath() / "rootfs";
    libsarus::UserIdentity userIdentity;

    size_t mount_flags = 0;

    {
        // Create source file as a character device file with 666 file mode
        auto testDeviceFile = testDir.getPath() / "sarusTestDevice0";
        auto majorID = 511u;
        auto minorID = 511u;
        aux::filesystem::createCharacterDeviceFile(testDeviceFile, majorID, minorID);

        auto mountObject = libsarus::Mount{testDeviceFile, testDeviceFile, mount_flags, rootfsDir, userIdentity};
        auto devAccess = libsarus::DeviceAccess("rwm");

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
        aux::filesystem::createBlockDeviceFile(testDeviceFile, majorID, minorID);

        auto mountObject = libsarus::Mount{testDeviceFile, testDeviceFile, mount_flags, rootfsDir, userIdentity};
        auto devAccess = libsarus::DeviceAccess("rw");

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
    auto testDir = libsarus::PathRAII(
        libsarus::filesystem::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "deviceMount-test-performMount"));
    libsarus::filesystem::createFoldersIfNecessary(testDir.getPath());

    auto bundleDirRAII = libsarus::PathRAII{libsarus::filesystem::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("test-bundle-dir"))};
    const auto& bundleDir = bundleDirRAII.getPath();
    auto rootfsDir = bundleDir / "rootfs";
    libsarus::UserIdentity userIdentity;

    libsarus::filesystem::createFoldersIfNecessary(rootfsDir);

    auto sourceFile = testDir.getPath() / "sarusTestDevice0";
    auto destinationFile = boost::filesystem::path{"/dev/sarusTestDevice0"};

    auto majorID = 511u;
    auto minorID = 511u;
    aux::filesystem::createCharacterDeviceFile(sourceFile, majorID, minorID);

    size_t mount_flags = 0;
    auto mountObject = libsarus::Mount{sourceFile, destinationFile, mount_flags, rootfsDir, userIdentity};
    auto devAccess = libsarus::DeviceAccess("rwm");

    // perform the mount
    libsarus::DeviceMount{std::move(mountObject), devAccess}.performMount();
    CHECK(aux::filesystem::isSameBindMountedFile(sourceFile, rootfsDir / destinationFile));
    CHECK(libsarus::filesystem::getDeviceID(rootfsDir / destinationFile) == makedev(majorID, minorID));
    CHECK(libsarus::filesystem::getDeviceType(rootfsDir / destinationFile) == 'c');

    // cleanup
    CHECK(umount((rootfsDir / destinationFile).c_str()) == 0);
    boost::filesystem::remove(sourceFile);
}

}}

SARUS_UNITTEST_MAIN_FUNCTION();
