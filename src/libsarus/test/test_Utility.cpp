/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <array>
#include <unistd.h>
#include <sys/fsuid.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>

#include "aux/misc.hpp"
#include "aux/unitTestMain.hpp"
#include "libsarus/PathRAII.hpp"
#include "libsarus/Utility.hpp"


namespace libsarus {
namespace test {

TEST_GROUP(UtilityTestGroup) {
};

TEST(UtilityTestGroup, parseEnvironmentVariables) {
    // empty environment
    {
        auto env = std::array<char*, 1>{nullptr};
        auto map = libsarus::environment::parseVariables(env.data());
        CHECK(map.empty());
    }
    // non-empty environment
    {
        auto var0 = std::string{"key0="};
        auto var1 = std::string{"key1=value1"};
        auto env = std::array<char*, 3>{&var0[0], &var1[0], nullptr};
        auto actualMap = libsarus::environment::parseVariables(env.data());
        auto expectedMap = std::unordered_map<std::string, std::string>{
            {"key0", ""},
            {"key1", "value1"}
        };
        CHECK(actualMap == expectedMap);
    }
}

TEST(UtilityTestGroup, getEnvironmentVariable) {
    auto testKey = std::string{"SARUS_UNITTEST_GETVAR"};
    auto testValue = std::string{"dummy"};

    // test with variable unset
    CHECK_THROWS(libsarus::Error, libsarus::environment::getVariable(testKey));

    // test with variable set
    libsarus::environment::setVariable(testKey, testValue);
    auto fallbackValue = std::string("fallback");
    CHECK_EQUAL(libsarus::environment::getVariable(testKey), testValue);
}

TEST(UtilityTestGroup, setEnvironmentVariable) {
    auto testKey = std::string{"SARUS_UNITTEST_SETVAR"};
    auto testValue = std::string{"dummy"};

    // test with variable not set
    if (unsetenv(testKey.c_str()) != 0) {
        auto message = boost::format("Error un-setting the variable used by the test: %s") % strerror(errno);
        FAIL(message.str().c_str());
    }
    libsarus::environment::setVariable(testKey, testValue);
    char *envValue = getenv(testKey.c_str());
    if (envValue == nullptr) {
        auto message = boost::format("Error getting the test variable from the environment");
        FAIL(message.str().c_str());
    }
    CHECK_EQUAL(std::string(envValue), testValue);

    // test overwrite with variable set
    testValue = std::string{"overwrite_dummy"};
    libsarus::environment::setVariable(testKey, testValue);
    envValue = getenv(testKey.c_str());
    if (envValue == nullptr) {
        auto message = boost::format("Error getting the test variable from the environment");
        FAIL(message.str().c_str());
    }
    CHECK_EQUAL(std::string(envValue), testValue);
}

TEST(UtilityTestGroup, parseKeyValuePair) {
    auto pair = libsarus::string::parseKeyValuePair("key=value");
    CHECK_EQUAL(pair.first, std::string("key"));
    CHECK_EQUAL(pair.second, std::string("value"));

    // key only
    pair = libsarus::string::parseKeyValuePair("key_only");
    CHECK_EQUAL(pair.first, std::string("key_only"));
    CHECK_EQUAL(pair.second, std::string(""));

    // no value after separator
    pair = libsarus::string::parseKeyValuePair("key=");
    CHECK_EQUAL(pair.first, std::string("key"));
    CHECK_EQUAL(pair.second, std::string(""));

    // non-default separator
    pair = libsarus::string::parseKeyValuePair("key:value", ':');
    CHECK_EQUAL(pair.first, std::string("key"));
    CHECK_EQUAL(pair.second, std::string("value"));

    // empty input
    CHECK_THROWS(libsarus::Error, libsarus::string::parseKeyValuePair(""));

    // missing key
    CHECK_THROWS(libsarus::Error, libsarus::string::parseKeyValuePair("=value"));
}

TEST(UtilityTestGroup, switchIdentity) {
    auto testDirRAII = libsarus::PathRAII{ "./sarus-test-switchIdentity" };
    libsarus::filesystem::createFileIfNecessary(testDirRAII.getPath() / "file", 0, 0);
    boost::filesystem::permissions(testDirRAII.getPath(), boost::filesystem::owner_all);

    uid_t unprivilegedUid;
    gid_t unprivilegedGid;
    std::tie(unprivilegedUid, unprivilegedGid) = aux::misc::getNonRootUserIds();
    auto unprivilegedIdentity = libsarus::UserIdentity{unprivilegedUid, unprivilegedGid, {}};

    libsarus::process::switchIdentity(unprivilegedIdentity);

    // Check identity change
    CHECK_EQUAL(geteuid(), unprivilegedIdentity.uid);
    CHECK_EQUAL(getegid(), unprivilegedIdentity.gid);

    // Check it's not possible to read root-owned files or write in root-owned dirs
    CHECK_THROWS(std::exception, boost::filesystem::exists(testDirRAII.getPath() / "file"));
    CHECK_THROWS(std::exception, libsarus::filesystem::createFileIfNecessary(testDirRAII.getPath() / "file_fail"));

    auto rootIdentity = libsarus::UserIdentity{};
    libsarus::process::switchIdentity(rootIdentity);

    CHECK_EQUAL(geteuid(), 0);
    CHECK_EQUAL(getegid(), 0);
    CHECK(boost::filesystem::exists(testDirRAII.getPath() / "file"));
}

TEST(UtilityTestGroup, setFilesystemUid) {
    // switch to unprivileged user
    uid_t unprivilegedUid;
    gid_t unprivilegedGid;
    std::tie(unprivilegedUid, unprivilegedGid) = aux::misc::getNonRootUserIds();
    auto unprivilegedIdentity = libsarus::UserIdentity{unprivilegedUid, unprivilegedGid, {}};
    auto rootIdentity = libsarus::UserIdentity{};

    libsarus::process::setFilesystemUid(unprivilegedIdentity);

    // check identity change
    CHECK_EQUAL(getuid(), rootIdentity.uid);
    CHECK_EQUAL(getgid(), rootIdentity.gid);
    CHECK_EQUAL(geteuid(), rootIdentity.uid);
    CHECK_EQUAL(getegid(), rootIdentity.gid);
    CHECK_EQUAL(setfsuid(-1), unprivilegedIdentity.uid);

    // switch back to privileged fsuid
    libsarus::process::setFilesystemUid(rootIdentity);

    // check identity change
    CHECK_EQUAL(getuid(), rootIdentity.uid);
    CHECK_EQUAL(getgid(), rootIdentity.gid);
    CHECK_EQUAL(geteuid(), rootIdentity.uid);
    CHECK_EQUAL(getegid(), rootIdentity.gid);
    CHECK_EQUAL(setfsuid(-1), rootIdentity.uid);
}

TEST(UtilityTestGroup, executeCommand) {
    CHECK_EQUAL(libsarus::process::executeCommand("printf stdout"), std::string{"stdout"});
    CHECK_EQUAL(libsarus::process::executeCommand("bash -c 'printf stderr >&2'"), std::string{"stderr"});
    CHECK_THROWS(libsarus::Error, libsarus::process::executeCommand("false"));
    CHECK_THROWS(libsarus::Error, libsarus::process::executeCommand("command-that-doesnt-exist-xyz"));
}

TEST(UtilityTestGroup, makeUniquePathWithRandomSuffix) {
    auto path = boost::filesystem::path{"/tmp/file"};
    auto uniquePath = libsarus::filesystem::makeUniquePathWithRandomSuffix(path);

    auto matches = boost::cmatch{};
    auto expectedRegex = boost::regex("^/tmp/file-[A-z]{16}$");
    CHECK(boost::regex_match(uniquePath.string().c_str(), matches, expectedRegex));
}

TEST(UtilityTestGroup, createFoldersIfNecessary) {
    libsarus::filesystem::createFoldersIfNecessary("/tmp/grandparent/parent/child");
    CHECK((libsarus::filesystem::getOwner("/tmp/grandparent/parent") == std::tuple<uid_t, gid_t>{0, 0}));
    CHECK((libsarus::filesystem::getOwner("/tmp/grandparent/parent/child") == std::tuple<uid_t, gid_t>{0, 0}));
    boost::filesystem::remove_all("/tmp/grandparent");

    libsarus::filesystem::createFoldersIfNecessary("/tmp/grandparent/parent/child", 1000, 1000);
    CHECK((libsarus::filesystem::getOwner("/tmp/grandparent/parent") == std::tuple<uid_t, gid_t>{1000, 1000}));
    CHECK((libsarus::filesystem::getOwner("/tmp/grandparent/parent/child")  == std::tuple<uid_t, gid_t>{1000, 1000}));
    boost::filesystem::remove_all("/tmp/grandparent");
}

TEST(UtilityTestGroup, createFileIfNecessary) {
    libsarus::filesystem::createFileIfNecessary("/tmp/testFile");
    CHECK((libsarus::filesystem::getOwner("/tmp/testFile") == std::tuple<uid_t, gid_t>{0, 0}));
    boost::filesystem::remove_all("/tmp/testFile");

    libsarus::filesystem::createFileIfNecessary("/tmp/testFile", 1000, 1000);
    CHECK((libsarus::filesystem::getOwner("/tmp/testFile") == std::tuple<uid_t, gid_t>{1000, 1000}));
    boost::filesystem::remove_all("/tmp/testFile");
}

TEST(UtilityTestGroup, copyFile) {
    auto testDirRAII = libsarus::PathRAII{ "./sarus-test-copyFile" };
    const auto& testDir = testDirRAII.getPath();
    libsarus::filesystem::createFileIfNecessary(testDir / "src");

    // implicit owner
    libsarus::filesystem::copyFile(testDir / "src", testDir / "dst");
    CHECK((libsarus::filesystem::getOwner(testDir / "dst") == std::tuple<uid_t, gid_t>{0, 0}));

    // explicit owner + overwrite existing file
    libsarus::filesystem::copyFile(testDir / "src", testDir / "dst", 1000, 1000);
    CHECK((libsarus::filesystem::getOwner(testDir / "dst") == std::tuple<uid_t, gid_t>{1000, 1000}));

    // explicit owner + non-existing directory
    libsarus::filesystem::copyFile(testDir / "src", testDir / "non-existing-folder/dst", 1000, 1000);
    CHECK((libsarus::filesystem::getOwner(testDir / "non-existing-folder") == std::tuple<uid_t, gid_t>{1000, 1000}));
    CHECK((libsarus::filesystem::getOwner(testDir / "non-existing-folder/dst") == std::tuple<uid_t, gid_t>{1000, 1000}));

    boost::filesystem::remove_all(testDir);
}

TEST(UtilityTestGroup, copyFolder) {
    libsarus::filesystem::createFoldersIfNecessary("/tmp/src-folder/subfolder");
    libsarus::filesystem::createFileIfNecessary("/tmp/src-folder/file0");
    libsarus::filesystem::createFileIfNecessary("/tmp/src-folder/subfolder/file1");

    libsarus::filesystem::copyFolder("/tmp/src-folder", "/tmp/dst-folder");
    CHECK((libsarus::filesystem::getOwner("/tmp/dst-folder/file0") == std::tuple<uid_t, gid_t>{0, 0}));
    CHECK((libsarus::filesystem::getOwner("/tmp/dst-folder/subfolder/file1") == std::tuple<uid_t, gid_t>{0, 0}));
    boost::filesystem::remove_all("/tmp/dst-folder");

    libsarus::filesystem::copyFolder("/tmp/src-folder", "/tmp/dst-folder", 1000, 1000);
    CHECK((libsarus::filesystem::getOwner("/tmp/dst-folder/file0") == std::tuple<uid_t, gid_t>{1000, 1000}));
    CHECK((libsarus::filesystem::getOwner("/tmp/dst-folder/subfolder/file1") == std::tuple<uid_t, gid_t>{1000, 1000}));
    boost::filesystem::remove_all("/tmp/dst-folder");
    boost::filesystem::remove_all("/tmp/src-folder");
}

TEST(UtilityTestGroup, countFilesInDirectory) {
    // nominal usage
    {
        auto testDir = boost::filesystem::path("/tmp/file-count-test");
        libsarus::filesystem::createFoldersIfNecessary(testDir);
        libsarus::filesystem::createFileIfNecessary(testDir / "file1");
        libsarus::filesystem::createFileIfNecessary(testDir / "file2");
        libsarus::filesystem::createFileIfNecessary(testDir / "file3");
        libsarus::filesystem::createFileIfNecessary(testDir / "file4");
        CHECK_EQUAL(libsarus::filesystem::countFilesInDirectory(testDir), 4);

        boost::filesystem::remove(testDir / "file1");
        boost::filesystem::remove(testDir / "file4");
        CHECK_EQUAL(libsarus::filesystem::countFilesInDirectory(testDir), 2);

        boost::filesystem::remove_all(testDir);
    }
    // non-existing directory
    {
        CHECK_THROWS(libsarus::Error, libsarus::filesystem::countFilesInDirectory("/tmp/" + libsarus::string::generateRandom(16)));
    }
    // non-directory argument
    {
        auto testFile = libsarus::PathRAII{"/tmp/file-count-test.txt"};
        libsarus::filesystem::createFileIfNecessary(testFile.getPath());
        CHECK_THROWS(libsarus::Error, libsarus::filesystem::countFilesInDirectory(testFile.getPath()));
    }
}

TEST(UtilityTestGroup, parseMap) {
    // empty list
    {
        auto map = libsarus::string::parseMap("");
        CHECK(map.empty());
    }
    // one key-value pair
    {
        auto list = "key0=value0";
        auto map = libsarus::string::parseMap(list);
        CHECK_EQUAL(map.size(), 1);
        CHECK_EQUAL(map["key0"], std::string{"value0"});
    }
    // two key-value pairs
    {
        auto list = "key0=value0,key1=value1";
        auto map = libsarus::string::parseMap(list);
        CHECK_EQUAL(map.size(), 2);
        CHECK_EQUAL(map["key0"], std::string{"value0"});
        CHECK_EQUAL(map["key1"], std::string{"value1"});
    }
    // key only (no value associated)
    {
        auto list = "key_only";
        auto map = libsarus::string::parseMap(list);
        CHECK_EQUAL(map.size(), 1);
        CHECK(map["key_only"] == "");
    }
    {
        auto list = "key_only_at_begin,key=value";
        auto map = libsarus::string::parseMap(list);
        CHECK_EQUAL(map.size(), 2);
        CHECK(map["key_only_at_begin"] == "");
        CHECK(map["key"] == "value");
    }
    {
        auto list = "key=value,key_only_at_end";
        auto map = libsarus::string::parseMap(list);
        CHECK_EQUAL(map.size(), 2);
        CHECK(map["key"] == "value");
        CHECK(map["key_only_at_end"] == "");
    }
    {
        auto list = "key_only0,key_only1";
        auto map = libsarus::string::parseMap(list);
        CHECK_EQUAL(map.size(), 2);
        CHECK(map["key_only0"] == "");
        CHECK(map["key_only1"] == "");
    }
    // missing key error
    {
        auto list = ",key=value";
        CHECK_THROWS(libsarus::Error, libsarus::string::parseMap(list));
    }
    {
        auto list = "key0=value0,,key1=value1";
        CHECK_THROWS(libsarus::Error, libsarus::string::parseMap(list));
    }
    {
        auto list = "key0=value0,";
        CHECK_THROWS(libsarus::Error, libsarus::string::parseMap(list));
    }
    // repeated key error
    {
        auto list = "key0=value0,key0=value1";
        CHECK_THROWS(libsarus::Error, libsarus::string::parseMap(list));
    }
    // too many values error, a.k.a. repeated kv separator
    {
        auto list = "key0=value0=value1";
        CHECK_THROWS(libsarus::Error, libsarus::string::parseMap(list));
    }
}

TEST(UtilityTestGroup, realpathWithinRootfs) {
    auto path = libsarus::PathRAII{libsarus::filesystem::makeUniquePathWithRandomSuffix("/tmp/sarus-rootfs")};
    const auto& rootfs = path.getPath();

    libsarus::filesystem::createFoldersIfNecessary(rootfs / "dir0/dir1");
    libsarus::filesystem::createFoldersIfNecessary(rootfs / "dirX");
    libsarus::filesystem::createFileIfNecessary(rootfs / "dir0/dir1/file");

    // folder
    CHECK(libsarus::filesystem::realpathWithinRootfs(rootfs, "/dir0/dir1") == "/dir0/dir1");

    // file
    CHECK(libsarus::filesystem::realpathWithinRootfs(rootfs, "/dir0/dir1/file") == "/dir0/dir1/file");

    // relative symlink
    CHECK_EQUAL(symlink("../../dir0/dir1", (rootfs / "dir0/dir1/link_relative").string().c_str()), 0);
    CHECK(libsarus::filesystem::realpathWithinRootfs(rootfs, "/dir0/dir1/link_relative") == "/dir0/dir1");

    // relative symlink that spills (out of rootfs)
    CHECK_EQUAL(symlink("../../../../dir0/dir1", (rootfs / "dir0/dir1/link_relative_that_spills").string().c_str()), 0);
    CHECK(libsarus::filesystem::realpathWithinRootfs(rootfs, "/dir0/dir1/link_relative_that_spills") == "/dir0/dir1");

    // relative symlink recursive
    CHECK_EQUAL(symlink("../../dir0/dir1/link_relative/dir2/dir3", (rootfs / "dir0/dir1/link_relative_recursive").string().c_str()), 0);
    CHECK(libsarus::filesystem::realpathWithinRootfs(rootfs, "/dir0/dir1/link_relative_recursive") == "/dir0/dir1/dir2/dir3");

    // relative symlink recursive that spills (out of rootfs)
    CHECK_EQUAL(symlink("../../../dir0/dir1/link_relative_that_spills/dir2/dir3", (rootfs / "dir0/dir1/link_relative_recursive_that_spills").string().c_str()), 0);
    CHECK(libsarus::filesystem::realpathWithinRootfs(rootfs, "/dir0/dir1/link_relative_recursive_that_spills") == "/dir0/dir1/dir2/dir3");

    // absolute symlink
    CHECK_EQUAL(symlink("/dir0/dir1", (rootfs / "dir0/dir1/link_absolute").string().c_str()), 0);
    CHECK(libsarus::filesystem::realpathWithinRootfs(rootfs, "/dir0/dir1/link_absolute") == "/dir0/dir1");

    // absolute symlink that spills (out of rootfs)
    CHECK_EQUAL(symlink("/dir0/dir1/../../../../dir0/dir1", (rootfs / "dir0/dir1/link_absolute_that_spills").string().c_str()), 0);
    CHECK(libsarus::filesystem::realpathWithinRootfs(rootfs, "/dir0/dir1/link_absolute_that_spills") == "/dir0/dir1");

    // absolute symlink recursive
    CHECK_EQUAL(symlink("/dir0/dir1/link_absolute/dir2/dir3", (rootfs / "dir0/dir1/link_absolute_recursive").string().c_str()), 0);
    CHECK(libsarus::filesystem::realpathWithinRootfs(rootfs, "/dir0/dir1/link_absolute_recursive") == "/dir0/dir1/dir2/dir3");

    // absolute symlink recursive that spills (out of rootfs)
    CHECK_EQUAL(symlink("/dir0/dir1/link_absolute_that_spills/dir2/dir3", (rootfs / "dir0/dir1/link_absolute_recursive_that_spills").string().c_str()), 0);
    CHECK(libsarus::filesystem::realpathWithinRootfs(rootfs, "/dir0/dir1/link_absolute_recursive_that_spills") == "/dir0/dir1/dir2/dir3");

    // absolute symlink sharing no part of the path with the target
    CHECK_EQUAL(symlink("/dir0/dir1", (rootfs / "dirX/link_absolute_with_no_common_path").string().c_str()), 0);
    CHECK(libsarus::filesystem::realpathWithinRootfs(rootfs, "/dirX/link_absolute_with_no_common_path") == "/dir0/dir1");
}

TEST(UtilityTestGroup, getSharedLibLinkerName) {
    CHECK(libsarus::sharedlibs::getLinkerName("file.so") == "file.so");
    CHECK(libsarus::sharedlibs::getLinkerName("file.so.1") == "file.so");
    CHECK(libsarus::sharedlibs::getLinkerName("file.so.1.0") == "file.so");
    CHECK(libsarus::sharedlibs::getLinkerName("file.so.1.0.0") == "file.so");

    CHECK_THROWS(libsarus::Error, libsarus::sharedlibs::getLinkerName("not-a-shared-lib"));
    CHECK_THROWS(libsarus::Error, libsarus::sharedlibs::getLinkerName("not-a-shared-lib.soa"));
}

TEST(UtilityTestGroup, isSharedLib) {
    CHECK(libsarus::filesystem::isSharedLib("/dir/libc.so") == true);
    CHECK(libsarus::filesystem::isSharedLib("libc.so") == true);
    CHECK(libsarus::filesystem::isSharedLib("libc.so.1") == true);
    CHECK(libsarus::filesystem::isSharedLib("libc.so.1.2") == true);

    CHECK(libsarus::filesystem::isSharedLib("libc") == false);
    CHECK(libsarus::filesystem::isSharedLib("libc.s") == false);
    CHECK(libsarus::filesystem::isSharedLib("ld.so.conf") == false);
    CHECK(libsarus::filesystem::isSharedLib("ld.so.cache") == false);
}

TEST(UtilityTestGroup, parseSharedLibAbi) {
    CHECK_THROWS(libsarus::Error, libsarus::sharedlibs::parseAbi("invalid"));
    CHECK(libsarus::sharedlibs::parseAbi("libc.so") == (std::vector<std::string>{}));
    CHECK(libsarus::sharedlibs::parseAbi("libc.so.1") == (std::vector<std::string>{"1"}));
    CHECK(libsarus::sharedlibs::parseAbi("libc.so.1.2") == (std::vector<std::string>{"1", "2"}));
    CHECK(libsarus::sharedlibs::parseAbi("libc.so.1.2.3") == (std::vector<std::string>{"1", "2", "3"}));
    CHECK(libsarus::sharedlibs::parseAbi("libc.so.1.2.3rc1") == (std::vector<std::string>{"1", "2", "3rc1"}));

    CHECK(libsarus::sharedlibs::parseAbi("libfoo.so.0") == (std::vector<std::string>{"0"}));
}

TEST(UtilityTestGroup, resolveSharedLibAbi) {
    auto testDirRaii = libsarus::PathRAII{
        libsarus::filesystem::makeUniquePathWithRandomSuffix("/tmp/sarus-test-utility-resolveSharedLibAbi")
    };
    const auto& testDir = testDirRaii.getPath();

    // invalid library filename
    libsarus::filesystem::createFileIfNecessary(testDir / "invalid");
    CHECK_THROWS(libsarus::Error, libsarus::sharedlibs::resolveAbi(testDir / "invalid"));

    // libtest.so
    libsarus::filesystem::createFileIfNecessary(testDir / "libtest.so");
    CHECK(libsarus::sharedlibs::resolveAbi(testDir / "libtest.so") == std::vector<std::string>{});

    // libtest.so.1
    libsarus::filesystem::createFileIfNecessary(testDir / "libtest.so.1");
    CHECK(libsarus::sharedlibs::resolveAbi(testDir / "libtest.so.1") == std::vector<std::string>{"1"});

    // libtest_symlink.so.1 -> libtest_symlink.so.1.2
    libsarus::filesystem::createFileIfNecessary(testDir / "libtest_symlink.so.1.2");
    boost::filesystem::create_symlink(testDir / "libtest_symlink.so.1.2", testDir / "libtest_symlink.so.1");
    CHECK(libsarus::sharedlibs::resolveAbi(testDir / "libtest_symlink.so.1") == (std::vector<std::string>{"1", "2"}));

    // libtest_symlink.so.1.2.3 -> libtest_symlink.so.1.2
    boost::filesystem::create_symlink(testDir / "libtest_symlink.so.1.2", testDir / "libtest_symlink.so.1.2.3");
    CHECK(libsarus::sharedlibs::resolveAbi(testDir / "libtest_symlink.so.1.2.3") == (std::vector<std::string>{"1", "2", "3"}));

    // libtest_symlink.so -> libtest_symlink.so.1.2.3 -> libtest_symlink.so.1.2
    boost::filesystem::create_symlink(testDir / "libtest_symlink.so.1.2.3", testDir / "libtest_symlink.so");
    CHECK(libsarus::sharedlibs::resolveAbi(testDir / "libtest_symlink.so") == (std::vector<std::string>{"1", "2", "3"}));

    // subdir/libtest_symlink.so -> ../libtest_symlink.so.1.2.3 -> libtest_symlink.so.1.2
    libsarus::filesystem::createFoldersIfNecessary(testDir / "subdir");
    boost::filesystem::create_symlink("../libtest_symlink.so.1.2.3", testDir / "subdir/libtest_symlink.so");
    CHECK(libsarus::sharedlibs::resolveAbi(testDir / "subdir/libtest_symlink.so") == (std::vector<std::string>{"1", "2", "3"}));

    // /libtest_symlink_within_rootdir.so -> /subdir/libtest_symlink_within_rootdir.so.1 -> ../libtest_symlink_within_rootdir.so.1.2
    boost::filesystem::create_symlink("/subdir/libtest_symlink_within_rootdir.so.1", testDir / "libtest_symlink_within_rootdir.so");
    boost::filesystem::create_symlink("../libtest_symlink_within_rootdir.so.1.2", testDir / "/subdir/libtest_symlink_within_rootdir.so.1");
    libsarus::filesystem::createFileIfNecessary(testDir / "libtest_symlink_within_rootdir.so.1.2");
    CHECK(libsarus::sharedlibs::resolveAbi("/libtest_symlink_within_rootdir.so", testDir) == (std::vector<std::string>{"1", "2"}));

    // Some vendors have symlinks with incompatible major versions,
    // like libvdpau_nvidia.so.1 -> libvdpau_nvidia.so.440.33.01.
    // For these cases, we trust the vendor and resolve the Lib Abi to that of the symlink.
    // Note here we use libtest.so.1 as the "original lib file" and create a symlink to it.
    boost::filesystem::create_symlink(testDir / "libtest.so.1", testDir / "libtest.so.234.56");
    CHECK(libsarus::sharedlibs::resolveAbi(testDir / "libtest.so.234.56") == (std::vector<std::string>{"234", "56"}));

    boost::filesystem::create_symlink("../libtest.so.1.2", testDir / "subdir" / "libtest.so.234.56");
    CHECK(libsarus::sharedlibs::resolveAbi(testDir / "subdir" / "libtest.so.234.56") == (std::vector<std::string>{"234", "56"}));

    boost::filesystem::create_symlink("../libtest.so.1.2", testDir / "subdir" / "libtest.so.234");
    CHECK(libsarus::sharedlibs::resolveAbi(testDir / "subdir" / "libtest.so.234") == (std::vector<std::string>{"234"}));
}

TEST(UtilityTestGroup, getSharedLibSoname) {
    // FIXME: hard-coded relative path.
    auto dummyLibsDir = boost::filesystem::path{__FILE__}
        .parent_path()
        .parent_path()
        .parent_path()
        .parent_path() / "CI/dummy_libs";
    CHECK_EQUAL(libsarus::sharedlibs::getSoname(dummyLibsDir / "libc.so.6-host", "readelf"), std::string("libc.so.6"));
    CHECK_EQUAL(libsarus::sharedlibs::getSoname(dummyLibsDir / "ld-linux-x86-64.so.2-host", "readelf"), std::string("ld-linux-x86-64.so.2"));
    CHECK_THROWS(libsarus::Error, libsarus::sharedlibs::getSoname(dummyLibsDir / "lib_dummy_0.so", "readelf"));
}

TEST(UtilityTestGroup, isLibc) {
    // libc
    CHECK(libsarus::filesystem::isLibc("libc.so"));
    CHECK(libsarus::filesystem::isLibc("libc.so.6"));
    CHECK(libsarus::filesystem::isLibc("libc-2.29.so"));
    CHECK(libsarus::filesystem::isLibc("/libc.so"));
    CHECK(libsarus::filesystem::isLibc("../libc.so"));
    CHECK(libsarus::filesystem::isLibc("dir/libc.so"));
    CHECK(libsarus::filesystem::isLibc("dir/dir/libc.so"));
    CHECK(libsarus::filesystem::isLibc("/root/libc.so"));
    CHECK(libsarus::filesystem::isLibc("/root/dir/libc.so"));

    // not libc
    CHECK(!libsarus::filesystem::isLibc("libcl.so"));
    CHECK(!libsarus::filesystem::isLibc("libc_bogus.so"));
}

TEST(UtilityTestGroup, is64bitSharedLib) {
    // FIXME: hard-coded relative path.
    auto dummyLibsDir = boost::filesystem::path{__FILE__}
        .parent_path()
        .parent_path()
        .parent_path()
        .parent_path() / "CI/dummy_libs";
    CHECK(libsarus::sharedlibs::is64bitSharedLib(dummyLibsDir / "libc.so.6-host", "readelf"));
    CHECK(libsarus::sharedlibs::is64bitSharedLib(dummyLibsDir / "ld-linux-x86-64.so.2-host", "readelf"));
    CHECK(!libsarus::sharedlibs::is64bitSharedLib(dummyLibsDir / "libc.so.6-32bit-container", "readelf"));
}

TEST(UtilityTestGroup, serializeJSON) {
    namespace rj = rapidjson;
    auto json = rj::Document{rj::kObjectType};
    auto& allocator = json.GetAllocator();

    json.AddMember("string", rj::Value{"stringValue", allocator}, allocator);
    json.AddMember("int", rj::Value{11}, allocator);
    json.AddMember("array", rj::Value{rj::kArrayType}, allocator);
    json["array"].PushBack(rj::Value{0}, allocator);
    json["array"].PushBack(rj::Value{1}, allocator);
    json["array"].PushBack(rj::Value{2}, allocator);

    auto actual = libsarus::json::serialize(json);
    auto expected = std::string{"{\"string\":\"stringValue\",\"int\":11,\"array\":[0,1,2]}"};

    CHECK_EQUAL(libsarus::string::removeWhitespaces(actual), expected);
}

TEST(UtilityTestGroup, setCpuAffinity_invalid_argument) {
    CHECK_THROWS(libsarus::Error, libsarus::process::setCpuAffinity({})); // no CPUs
}

TEST(UtilityTestGroup, getCpuAffinity_setCpuAffinity) {
    auto initialCpus = libsarus::process::getCpuAffinity();

    if(initialCpus.size() <= 1) {
        std::cerr << "Skipping CPU affinity unit test. Not enough CPUs available" << std::endl;
        return;
    }

    // set new affinity (removing one CPU)
    auto newCpus = initialCpus;
    newCpus.pop_back();
    libsarus::process::setCpuAffinity(newCpus);

    // check
    CHECK(libsarus::process::getCpuAffinity() == newCpus);

    // restore initial affinity
    libsarus::process::setCpuAffinity(initialCpus);
}

}}

SARUS_UNITTEST_MAIN_FUNCTION();
