/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <ios>
#include <string>
#include <vector>
#include <sys/sysmacros.h>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <rapidjson/document.h>

#include "common/Utility.hpp"
#include "common/PathRAII.hpp"
#include "hooks/common/Utility.hpp"
#include "test_utility/Misc.hpp"
#include "test_utility/OCIHooks.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace hooks {
namespace common {
namespace test {

TEST_GROUP(HooksUtilityTestGroup) {
};

TEST(HooksUtilityTestGroup, parseStateOfContainerFromStdin) {
    auto expectedPid = getpid();
    auto expectedBundleDir = sarus::common::PathRAII(
        sarus::common::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "hooks-test-bundle-dir"));
    sarus::common::createFoldersIfNecessary(expectedBundleDir.getPath());

    auto returnedBundleDir = boost::filesystem::path();
    pid_t returnedPid;

    test_utility::ocihooks::writeContainerStateToStdin(expectedBundleDir.getPath());
    std::tie(returnedBundleDir, returnedPid) = utility::parseStateOfContainerFromStdin();

    CHECK(returnedBundleDir == expectedBundleDir.getPath());
    CHECK_EQUAL(returnedPid, expectedPid);
}

TEST(HooksUtilityTestGroup, getEnvironmentVariableValueFromOCIBundle) {
    auto testBundleDir = sarus::common::PathRAII(
        sarus::common::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "hooks-test-bundle-dir"));
    sarus::common::createFoldersIfNecessary(testBundleDir.getPath());
    auto bundleConfigFile = testBundleDir.getPath() / "config.json";

    auto config = test_utility::ocihooks::createBaseConfigJSON(testBundleDir.getPath() / "rootfs",
                                                               test_utility::misc::getNonRootUserIds());
    auto& allocator = config.GetAllocator();

    // Variable set and non-empty
    {
        config["process"]["env"].PushBack("TEST_VAR_SET_NOEMPTY0=value0", allocator);
        config["process"]["env"].PushBack("TEST_VAR_SET_NOEMPTY1=value1", allocator);
        config["process"]["env"].PushBack("TEST_VAR_SET_NOEMPTY2=value2", allocator);
        sarus::common::writeJSON(config, bundleConfigFile);
        auto value = utility::getEnvironmentVariableValueFromOCIBundle("TEST_VAR_SET_NOEMPTY1", testBundleDir.getPath());
        CHECK(bool(value));
        CHECK(*value == std::string("value1"));
        config["process"]["env"].SetArray();
    }
    // Variable set and empty
    {
        config["process"]["env"].PushBack("TEST_VAR_SET_NOEMPTY0=value0", allocator);
        config["process"]["env"].PushBack("TEST_VAR_SET_EMPTY=", allocator);
        config["process"]["env"].PushBack("TEST_VAR_SET_NOEMPTY2=value2", allocator);
        sarus::common::writeJSON(config, bundleConfigFile);
        auto value = utility::getEnvironmentVariableValueFromOCIBundle("TEST_VAR_SET_EMPTY", testBundleDir.getPath());
        CHECK(bool(value));
        CHECK(*value == std::string(""));
        config["process"]["env"].SetArray();
    }
    // Variable not set
    {
        config["process"]["env"].PushBack("TEST_VAR_SET_NOEMPTY0=value0", allocator);
        config["process"]["env"].PushBack("TEST_VAR_SET_NOEMPTY2=value2", allocator);
        sarus::common::writeJSON(config, bundleConfigFile);
        auto value = utility::getEnvironmentVariableValueFromOCIBundle("TEST_VAR_NOT_SET", testBundleDir.getPath());
        CHECK_FALSE(value);
        config["process"]["env"].SetArray();
    }
}

TEST(HooksUtilityTestGroup, findSubsystemMountPaths) {
    auto testDir = sarus::common::PathRAII(
        sarus::common::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "hooks-test-subsys-mount-point"));
    auto mountinfoPath = testDir.getPath() / "proc" / "1" / "mountinfo";
    auto openmode = std::ios::out | std::ios::trunc;

    // single line corresponding to searched entry
    {
        auto expectedMountRoot =boost::filesystem::path("/");
        auto expectedMountPoint = boost::filesystem::path("/sys/fs/cgroup/devices");
        auto mountinfoContent = boost::format("34 25 0:29 %s %s rw,nosuid,nodev,noexec,relatime shared:15 - cgroup cgroup rw,devices")
                                              % expectedMountRoot.string() % expectedMountPoint.string();
        sarus::common::writeTextFile(mountinfoContent.str(), mountinfoPath, openmode);
        boost::filesystem::path returnedMountRoot;
        boost::filesystem::path returnedMountPoint;
        std::tie(returnedMountRoot, returnedMountPoint) = utility::findSubsystemMountPaths("devices", testDir.getPath(), 1);
        CHECK(boost::filesystem::equivalent(returnedMountRoot, expectedMountRoot));
        CHECK(boost::filesystem::equivalent(returnedMountPoint, expectedMountPoint));
    }
    // multiple cgroup lines
    {
        auto expectedMountRoot =boost::filesystem::path("/");
        auto expectedMountPoint = boost::filesystem::path("/sys/fs/cgroup/devices");
        auto mountinfoContent = boost::format(
                "36 25 0:31 / /sys/fs/cgroup/cpu,cpuacct rw,nosuid,nodev,noexec,relatime shared:17 - cgroup cgroup rw,cpuacct,cpu\n"
                "34 25 0:29 %s %s rw,nosuid,nodev,noexec,relatime shared:15 - cgroup cgroup rw,devices\n"
            )
            % expectedMountRoot.string() % expectedMountPoint.string();
        sarus::common::writeTextFile(mountinfoContent.str(), mountinfoPath, openmode);
        boost::filesystem::path returnedMountRoot;
        boost::filesystem::path returnedMountPoint;
        std::tie(returnedMountRoot, returnedMountPoint) = utility::findSubsystemMountPaths("devices", testDir.getPath(), 1);
        CHECK(boost::filesystem::equivalent(returnedMountRoot, expectedMountRoot));
        CHECK(boost::filesystem::equivalent(returnedMountPoint, expectedMountPoint));
    }
    // multiple lines with several fs types
    {
        auto expectedMountRoot =boost::filesystem::path("/");
        auto expectedMountPoint = boost::filesystem::path("/sys/fs/cgroup/devices");
        auto mountinfoContent = boost::format(
                "18 41 0:17 / /sys rw,nosuid,nodev,noexec,relatime shared:6 - sysfs sysfs rw,seclabel\n"
                "19 41 0:3 / /proc rw,nosuid,nodev,noexec,relatime shared:5 - proc proc rw\n"
                "36 25 0:31 / /sys/fs/cgroup/cpu,cpuacct rw,nosuid,nodev,noexec,relatime shared:17 - cgroup cgroup rw,cpuacct,cpu\n"
                "34 25 0:29 %s %s rw,nosuid,nodev,noexec,relatime shared:15 - cgroup cgroup rw,devices\n"
                "49 41 253:2 / /home rw,relatime shared:31 - xfs /dev/mapper/home rw,seclabel,attr2,inode64,noquota"
            )
            % expectedMountRoot.string() % expectedMountPoint.string();
        sarus::common::writeTextFile(mountinfoContent.str(), mountinfoPath, openmode);
        boost::filesystem::path returnedMountRoot;
        boost::filesystem::path returnedMountPoint;
        std::tie(returnedMountRoot, returnedMountPoint) = utility::findSubsystemMountPaths("devices", testDir.getPath(), 1);
        CHECK(boost::filesystem::equivalent(returnedMountRoot, expectedMountRoot));
        CHECK(boost::filesystem::equivalent(returnedMountPoint, expectedMountPoint));
    }
    // mount root different from filesystem root
    {
        auto expectedMountRoot =boost::filesystem::path("/another/mount/root");
        auto expectedMountPoint = boost::filesystem::path("/sys/fs/cgroup/devices");
        auto mountinfoContent = boost::format(
                "18 41 0:17 / /sys rw,nosuid,nodev,noexec,relatime shared:6 - sysfs sysfs rw,seclabel\n"
                "19 41 0:3 / /proc rw,nosuid,nodev,noexec,relatime shared:5 - proc proc rw\n"
                "36 25 0:31 / /sys/fs/cgroup/cpu,cpuacct rw,nosuid,nodev,noexec,relatime shared:17 - cgroup cgroup rw,cpuacct,cpu\n"
                "34 25 0:29 %s %s rw,nosuid,nodev,noexec,relatime shared:15 - cgroup cgroup rw,devices\n"
                "49 41 253:2 / /home rw,relatime shared:31 - xfs /dev/mapper/home rw,seclabel,attr2,inode64,noquota"
            )
            % expectedMountRoot.string() % expectedMountPoint.string();
        sarus::common::writeTextFile(mountinfoContent.str(), mountinfoPath, openmode);
        boost::filesystem::path returnedMountRoot;
        boost::filesystem::path returnedMountPoint;
        std::tie(returnedMountRoot, returnedMountPoint) = utility::findSubsystemMountPaths("devices", testDir.getPath(), 1);
        CHECK_EQUAL(returnedMountRoot.string(), expectedMountRoot.string());
        CHECK(boost::filesystem::equivalent(returnedMountPoint, expectedMountPoint));
    }
    // line with no optional fields
    {
        auto expectedMountRoot =boost::filesystem::path("/");
        auto expectedMountPoint = boost::filesystem::path("/sys/fs/cgroup/devices");
        auto mountinfoContent = boost::format(
                "18 41 0:17 / /sys rw,nosuid,nodev,noexec,relatime shared:6 - sysfs sysfs rw,seclabel\n"
                "19 41 0:3 / /proc rw,nosuid,nodev,noexec,relatime shared:5 - proc proc rw\n"
                "36 25 0:31 / /sys/fs/cgroup/cpu,cpuacct rw,nosuid,nodev,noexec,relatime shared:17 - cgroup cgroup rw,cpuacct,cpu\n"
                "34 25 0:29 %s %s rw,nosuid,nodev,noexec,relatime shared:15 - cgroup cgroup rw,devices\n"
                "49 41 253:2 / /home rw,relatime shared:31 - xfs /dev/mapper/home rw,seclabel,attr2,inode64,noquota"
            )
            % expectedMountRoot.string() % expectedMountPoint.string();
        sarus::common::writeTextFile(mountinfoContent.str(), mountinfoPath, openmode);
        boost::filesystem::path returnedMountRoot;
        boost::filesystem::path returnedMountPoint;
        std::tie(returnedMountRoot, returnedMountPoint) = utility::findSubsystemMountPaths("devices", testDir.getPath(), 1);
        CHECK(boost::filesystem::equivalent(returnedMountRoot, expectedMountRoot));
        CHECK(boost::filesystem::equivalent(returnedMountPoint, expectedMountPoint));
    }
    // malformed line on another entry (/proc missing superOptions and filesystem type)
    {
        auto expectedMountRoot =boost::filesystem::path("/");
        auto expectedMountPoint = boost::filesystem::path("/sys/fs/cgroup/devices");
        auto mountinfoContent = boost::format(
                "18 41 0:17 / /sys rw,nosuid,nodev,noexec,relatime shared:6 - sysfs sysfs rw,seclabel\n"
                "19 41 0:3 / /proc rw,nosuid,nodev,noexec,relatime shared:5 - proc proc rw\n"
                "36 25 0:31 / /sys/fs/cgroup/cpu,cpuacct rw,nosuid,nodev,noexec,relatime shared:17 - cgroup cgroup rw,cpuacct,cpu\n"
                "34 25 0:29 %s %s rw,nosuid,nodev,noexec,relatime shared:15 - cgroup cgroup rw,devices\n"
                "49 41 253:2 / /home rw,relatime shared:31 - xfs /dev/mapper/home rw,seclabel,attr2,inode64,noquota"
            )
            % expectedMountRoot.string() % expectedMountPoint.string();
        sarus::common::writeTextFile(mountinfoContent.str(), mountinfoPath, openmode);
        boost::filesystem::path returnedMountRoot;
        boost::filesystem::path returnedMountPoint;
        std::tie(returnedMountRoot, returnedMountPoint) = utility::findSubsystemMountPaths("devices", testDir.getPath(), 1);
        CHECK(boost::filesystem::equivalent(returnedMountRoot, expectedMountRoot));
        CHECK(boost::filesystem::equivalent(returnedMountPoint, expectedMountPoint));
    }
    // mount root resides in another cgroup namespace
    {
        auto expectedMountRoot =boost::filesystem::path("/..");
        auto expectedMountPoint = boost::filesystem::path("/sys/fs/cgroup/devices");
        auto mountinfoContent = boost::format(
                "18 41 0:17 / /sys rw,nosuid,nodev,noexec,relatime shared:6 - sysfs sysfs rw,seclabel\n"
                "19 41 0:3 / /proc rw,nosuid,nodev,noexec,relatime shared:5 - proc proc rw\n"
                "36 25 0:31 / /sys/fs/cgroup/cpu,cpuacct rw,nosuid,nodev,noexec,relatime shared:17 - cgroup cgroup rw,cpuacct,cpu\n"
                "34 25 0:29 %s %s rw,nosuid,nodev,noexec,relatime shared:15 - cgroup cgroup rw,devices\n"
                "49 41 253:2 / /home rw,relatime shared:31 - xfs /dev/mapper/home rw,seclabel,attr2,inode64,noquota"
            )
            % expectedMountRoot.string() % expectedMountPoint.string();
        sarus::common::writeTextFile(mountinfoContent.str(), mountinfoPath, openmode);
        CHECK_THROWS(sarus::common::Error, utility::findSubsystemMountPaths("devices", testDir.getPath(), 1));
    }
    // no line corresponding to searched entry
    {
        auto mountinfoContent = boost::format(
                "18 41 0:17 / /sys rw,nosuid,nodev,noexec,relatime shared:6 - sysfs sysfs rw,seclabel\n"
                "19 41 0:3 / /proc rw,nosuid,nodev,noexec,relatime shared:5 - proc proc rw\n"
                "36 25 0:31 / /sys/fs/cgroup/cpu,cpuacct rw,nosuid,nodev,noexec,relatime shared:17 - cgroup cgroup rw,cpuacct,cpu\n"
                "49 41 253:2 / /home rw,relatime shared:31 - xfs /dev/mapper/home rw,seclabel,attr2,inode64,noquota"
            );
        sarus::common::writeTextFile(mountinfoContent.str(), mountinfoPath, openmode);
        CHECK_THROWS(sarus::common::Error, utility::findSubsystemMountPaths("devices", testDir.getPath(), 1));
    }
    // malformed line corresponding to searched entry (missing superOptions and filesystem type)
    {
        auto expectedMountRoot =boost::filesystem::path("/");
        auto expectedMountPoint = boost::filesystem::path("/sys/fs/cgroup/devices");
        auto mountinfoContent = boost::format(
                "18 41 0:17 / /sys rw,nosuid,nodev,noexec,relatime shared:6 - sysfs sysfs rw,seclabel\n"
                "19 41 0:3 / /proc rw,nosuid,nodev,noexec,relatime shared:5 -proc proc rw\n"
                "36 25 0:31 / /sys/fs/cgroup/cpu,cpuacct rw,nosuid,nodev,noexec,relatime shared:17 - cgroup cgroup rw,cpuacct,cpu\n"
                "34 25 0:29 %s %s rw,nosuid,nodev,noexec,relatime shared:15 -  cgroup  \n"
                "49 41 253:2 / /home rw,relatime shared:31 - xfs /dev/mapper/home rw,seclabel,attr2,inode64,noquota"
            )
            % expectedMountRoot.string() % expectedMountPoint.string();
        sarus::common::writeTextFile(mountinfoContent.str(), mountinfoPath, openmode);
        CHECK_THROWS(sarus::common::Error, utility::findSubsystemMountPaths("devices", testDir.getPath(), 1));
    }
}

TEST(HooksUtilityTestGroup, findCgroupPathInHierarchy) {
    auto testDir = sarus::common::PathRAII(
        sarus::common::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "hooks-test-cgroup-relative-path"));
    auto procFilePath = testDir.getPath() / "proc" / "1" / "cgroup";
    auto openmode = std::ios::out | std::ios::trunc;

    // single line corresponding to searched entry
    {
        auto subsystemMountRoot = boost::filesystem::path("/");
        auto expectedPath = boost::filesystem::path("/user.slice");
        auto procFileContent = boost::format("11:devices:%s") % expectedPath.string();
        sarus::common::writeTextFile(procFileContent.str(), procFilePath, openmode);
        auto returnedPath = utility::findCgroupPathInHierarchy("devices", testDir.getPath(), subsystemMountRoot, 1);
        CHECK_EQUAL(returnedPath.string(), expectedPath.string());
    }
    // multiple lines
    {
        auto subsystemMountRoot = boost::filesystem::path("/");
        auto expectedPath = boost::filesystem::path("/user.slice");
        auto procFileContent = boost::format(
                "8:freezer:/\n"
                "7:devices:%s\n"
                "6:cpuacct,cpu:/\n"
                "5:cpuset:/"
            )
            % expectedPath.string();
        sarus::common::writeTextFile(procFileContent.str(), procFilePath, openmode);
        auto returnedPath = utility::findCgroupPathInHierarchy("devices", testDir.getPath(), subsystemMountRoot, 1);
        CHECK_EQUAL(returnedPath.string(), expectedPath.string());
    }
    // subsystem mount root is not filesystem root but not a prefix of cgroup path
    {
        auto subsystemMountRoot = boost::filesystem::path("/cgroup/container");
        auto expectedPath = boost::filesystem::path("/user.slice");
        auto procFileContent = boost::format(
                "8:freezer:/\n"
                "7:devices:%s\n"
                "6:cpuacct,cpu:/\n"
                "5:cpuset:/"
            )
            % expectedPath.string();
        sarus::common::writeTextFile(procFileContent.str(), procFilePath, openmode);
        auto returnedPath = utility::findCgroupPathInHierarchy("devices", testDir.getPath(), subsystemMountRoot, 1);
        CHECK_EQUAL(returnedPath.string(), expectedPath.string());
    }
    // subsystem mount root is not filesystem root and a prefix of cgroup path
    {
        auto subsystemMountRoot = boost::filesystem::path("/cgroup/container");
        auto expectedPath = boost::filesystem::path("/user.slice");
        auto cgroupInProcFile = boost::filesystem::path("/cgroup/container/user.slice");
        auto procFileContent = boost::format(
                "8:freezer:/\n"
                "7:devices:%s\n"
                "6:cpuacct,cpu:/\n"
                "5:cpuset:/"
            )
            % cgroupInProcFile.string();
        sarus::common::writeTextFile(procFileContent.str(), procFilePath, openmode);
        auto returnedPath = utility::findCgroupPathInHierarchy("devices", testDir.getPath(), subsystemMountRoot, 1);
        CHECK_EQUAL(returnedPath.string(), expectedPath.string());
    }
    // line with cgroup version 2 syntax
    // notice the line is put before the searched entry to test if the function correctly parses and skips over,
    // even if in reality /proc/[pid]/cgroup displays the entries in descending order of hierarchy ID (first field),
    // thus a cgroup version 2 line will always be at the bottom of the list on a real cgroup file.
    {
        auto subsystemMountRoot = boost::filesystem::path("/");
        auto expectedPath = boost::filesystem::path("/user.slice");
        auto procFileContent = boost::format(
                "0::/user.slice/user-1000.slice/session-1000.scope\n"
                "8:freezer:/\n"
                "7:devices:%s\n"
                "6:cpuacct,cpu:/\n"
                "5:cpuset:/"
            )
            % expectedPath.string();
        sarus::common::writeTextFile(procFileContent.str(), procFilePath, openmode);
        auto returnedPath = utility::findCgroupPathInHierarchy("devices", testDir.getPath(), subsystemMountRoot, 1);
        CHECK_EQUAL(returnedPath.string(), expectedPath.string());
    }
    // path is part of a hierarchy rooted in another cgroup namespace
    {
        auto subsystemMountRoot = boost::filesystem::path("/");
        auto expectedPath = boost::filesystem::path("/../user.slice");
        auto procFileContent = boost::format(
                "8:freezer:/\n"
                "7:devices:%s\n"
                "6:cpuacct,cpu:/\n"
                "5:cpuset:/"
            )
            % expectedPath.string();
        sarus::common::writeTextFile(procFileContent.str(), procFilePath, openmode);
        CHECK_THROWS(sarus::common::Error, utility::findCgroupPathInHierarchy("devices", testDir.getPath(), subsystemMountRoot, 1));
    }
    // no line corresponding to searched entry
    {
        auto subsystemMountRoot = boost::filesystem::path("/");
        auto procFileContent = boost::format(
                "8:freezer:/\n"
                "6:cpuacct,cpu:/\n"
                "5:cpuset:/"
            );
        sarus::common::writeTextFile(procFileContent.str(), procFilePath, openmode);
        CHECK_THROWS(sarus::common::Error, utility::findCgroupPathInHierarchy("devices", testDir.getPath(), subsystemMountRoot, 1));
    }
}

TEST(HooksUtilityTestGroup, findCgroupPath) {
    auto testDir = sarus::common::PathRAII(
        sarus::common::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "hooks-test-cgroup-path"));

    // prepare mock /prod/[pid]/mountinfo file
    auto mountinfoPath = testDir.getPath() / "proc" / "1" / "mountinfo";
    auto mountPointPath = boost::filesystem::path(testDir.getPath() / "/sys/fs/cgroup/devices");
    auto mountinfoContent = boost::format(
            "18 41 0:17 / /sys rw,nosuid,nodev,noexec,relatime shared:6 - sysfs sysfs rw,seclabel\n"
            "19 41 0:3 / /proc rw,nosuid,nodev,noexec,relatime shared:5 - proc proc rw\n"
            "36 25 0:31 / /sys/fs/cgroup/cpu,cpuacct rw,nosuid,nodev,noexec,relatime shared:17 - cgroup cgroup rw,cpuacct,cpu\n"
            "34 25 0:29 / %s rw,nosuid,nodev,noexec,relatime shared:15 - cgroup cgroup rw,devices\n"
            "49 41 253:2 / /home rw,relatime shared:31 - xfs /dev/mapper/home rw,seclabel,attr2,inode64,noquota"
        )
        % mountPointPath.string();
    sarus::common::writeTextFile(mountinfoContent.str(), mountinfoPath);

    // prepare mock /prod/[pid]/cgroup file
    auto procFilePath = testDir.getPath() / "proc" / "1" / "cgroup";
    auto cgroupRelativePath = boost::filesystem::path("/user.slice");
    auto procFileContent = boost::format(
            "8:freezer:/\n"
            "7:devices:%s\n"
            "6:cpuacct,cpu:/\n"
            "5:cpuset:/"
        )
        % cgroupRelativePath.string();
    sarus::common::writeTextFile(procFileContent.str(), procFilePath);

    auto expectedPath = mountPointPath / cgroupRelativePath;

    // test with expected path not existing
    CHECK_THROWS(sarus::common::Error, utility::findCgroupPath("devices", testDir.getPath(), 1));

    // test with expected path existing
    sarus::common::createFoldersIfNecessary(expectedPath);
    auto returnedPath = utility::findCgroupPath("devices", testDir.getPath(), 1);
    CHECK(boost::filesystem::equivalent(returnedPath, expectedPath));
}

TEST(HooksUtilityTestGroup, whitelistDeviceInCgroup) {
    auto testDir = sarus::common::PathRAII(
        sarus::common::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "hooks-test-whitelist-device"));

    auto allowFile = testDir.getPath() / "devices.allow";
    sarus::common::createFileIfNecessary(allowFile);

    // regular operation
    utility::whitelistDeviceInCgroup(testDir.getPath(), "/dev/null");
    auto expectedDeviceID = sarus::common::getDeviceID("/dev/null");
    auto expectedEntry = boost::format("c %u:%u rw") % major(expectedDeviceID) % minor(expectedDeviceID);
    auto writtenEntry = sarus::common::readFile(allowFile);
    CHECK_EQUAL(writtenEntry, expectedEntry.str());

    // deviceFile argument is not a device
    auto dummyFile = testDir.getPath() / "dummy";
    sarus::common::createFileIfNecessary(dummyFile);
    CHECK_THROWS(sarus::common::Error, utility::whitelistDeviceInCgroup(testDir.getPath(), dummyFile));
}

TEST(HooksUtilityTestGroup, parseLibcVersionFromLddOutput) {
    CHECK((std::tuple<unsigned int, unsigned int>{2, 34} == utility::parseLibcVersionFromLddOutput(
            "ldd (GNU libc) 2.34\n"
            "Copyright (C) 2021 Free Software Foundation, Inc.\n"
            "This is free software; see the source for copying conditions.  There is NO\n"
            "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
            "Written by Roland McGrath and Ulrich Drepper.")));
    CHECK((std::tuple<unsigned int, unsigned int>{2, 31} == utility::parseLibcVersionFromLddOutput("ldd (Ubuntu GLIBC 2.31-0ubuntu9.2) 2.31")));
    CHECK((std::tuple<unsigned int, unsigned int>{0, 0} == utility::parseLibcVersionFromLddOutput("ldd (GNU libc) 0.0")));
    CHECK((std::tuple<unsigned int, unsigned int>{100, 100} == utility::parseLibcVersionFromLddOutput("ldd (GNU libc) 100.100")));
}

}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
