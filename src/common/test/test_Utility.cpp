/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>

#include "common/PathRAII.hpp"
#include "common/Utility.hpp"
#include "common/PathRAII.hpp"
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(UtilityTestGroup) {
};

TEST(UtilityTestGroup, parseEnvironmentVariables) {
    // empty environment
    {
        auto env = std::array<char*, 1>{nullptr};
        auto map = common::parseEnvironmentVariables(env.data());
        CHECK(map.empty());
    }
    // non-empty environment
    {
        auto var0 = std::string{"key0="};
        auto var1 = std::string{"key1=value1"};
        auto env = std::array<char*, 3>{&var0[0], &var1[0], nullptr};
        auto actualMap = common::parseEnvironmentVariables(env.data());
        auto expectedMap = std::unordered_map<std::string, std::string>{
            {"key0", ""},
            {"key1", "value1"}
        };
        CHECK(actualMap == expectedMap);
    }
}

TEST(UtilityTestGroup, executeCommand) {
    CHECK_EQUAL(common::executeCommand("printf stdout"), std::string{"stdout"});
    CHECK_EQUAL(common::executeCommand("bash -c 'printf stderr >&2'"), std::string{"stderr"});
    CHECK_THROWS(common::Error, common::executeCommand("false"));
    CHECK_THROWS(common::Error, common::executeCommand("command-that-doesnt-exist-xyz"));
}

TEST(UtilityTestGroup, makeUniquePathWithRandomSuffix) {
    auto path = boost::filesystem::path{"/tmp/file"};
    auto uniquePath = common::makeUniquePathWithRandomSuffix(path);

    auto matches = boost::cmatch{};
    auto expectedRegex = boost::regex("^/tmp/file-[A-z]{16}$");
    CHECK(boost::regex_match(uniquePath.string().c_str(), matches, expectedRegex));
}

TEST(UtilityTestGroup, createFoldersIfNecessary) {
    common::createFoldersIfNecessary("/tmp/grandparent/parent/child");
    CHECK((common::getOwner("/tmp/grandparent/parent") == std::tuple<uid_t, gid_t>{0, 0}));
    CHECK((common::getOwner("/tmp/grandparent/parent/child") == std::tuple<uid_t, gid_t>{0, 0}));
    boost::filesystem::remove_all("/tmp/grandparent");

    common::createFoldersIfNecessary("/tmp/grandparent/parent/child", 1000, 1000);
    CHECK((common::getOwner("/tmp/grandparent/parent") == std::tuple<uid_t, gid_t>{1000, 1000}));
    CHECK((common::getOwner("/tmp/grandparent/parent/child")  == std::tuple<uid_t, gid_t>{1000, 1000}));
    boost::filesystem::remove_all("/tmp/grandparent");
}

TEST(UtilityTestGroup, createFileIfNecessary) {
    common::createFileIfNecessary("/tmp/testFile");
    CHECK((common::getOwner("/tmp/testFile") == std::tuple<uid_t, gid_t>{0, 0}));
    boost::filesystem::remove_all("/tmp/testFile");

    common::createFileIfNecessary("/tmp/testFile", 1000, 1000);
    CHECK((common::getOwner("/tmp/testFile") == std::tuple<uid_t, gid_t>{1000, 1000}));
    boost::filesystem::remove_all("/tmp/testFile");
}

TEST(UtilityTestGroup, copyFile) {
    auto testDirRAII = common::PathRAII{ "./sarus-test-copyFile" };
    const auto& testDir = testDirRAII.getPath();
    common::createFileIfNecessary(testDir / "src");

    // implicit owner
    common::copyFile(testDir / "src", testDir / "dst");
    CHECK((common::getOwner(testDir / "dst") == std::tuple<uid_t, gid_t>{0, 0}));

    // explicit owner + overwrite existing file
    common::copyFile(testDir / "src", testDir / "dst", 1000, 1000);
    CHECK((common::getOwner(testDir / "dst") == std::tuple<uid_t, gid_t>{1000, 1000}));

    // explicit owner + non-existing directory
    common::copyFile(testDir / "src", testDir / "non-existing-folder/dst", 1000, 1000);
    CHECK((common::getOwner(testDir / "non-existing-folder") == std::tuple<uid_t, gid_t>{1000, 1000}));
    CHECK((common::getOwner(testDir / "non-existing-folder/dst") == std::tuple<uid_t, gid_t>{1000, 1000}));

    boost::filesystem::remove_all(testDir);
}

TEST(UtilityTestGroup, copyFolder) {
    common::createFoldersIfNecessary("/tmp/src-folder/subfolder");
    common::createFileIfNecessary("/tmp/src-folder/file0");
    common::createFileIfNecessary("/tmp/src-folder/subfolder/file1");

    common::copyFolder("/tmp/src-folder", "/tmp/dst-folder");
    CHECK((common::getOwner("/tmp/dst-folder/file0") == std::tuple<uid_t, gid_t>{0, 0}));
    CHECK((common::getOwner("/tmp/dst-folder/subfolder/file1") == std::tuple<uid_t, gid_t>{0, 0}));
    boost::filesystem::remove_all("/tmp/dst-folder");

    common::copyFolder("/tmp/src-folder", "/tmp/dst-folder", 1000, 1000);
    CHECK((common::getOwner("/tmp/dst-folder/file0") == std::tuple<uid_t, gid_t>{1000, 1000}));
    CHECK((common::getOwner("/tmp/dst-folder/subfolder/file1") == std::tuple<uid_t, gid_t>{1000, 1000}));
    boost::filesystem::remove_all("/tmp/dst-folder");
    boost::filesystem::remove_all("/tmp/src-folder");
}

TEST(UtilityTestGroup, countFilesInDirectory) {
    // nominal usage
    {
        auto testDir = boost::filesystem::path("/tmp/file-count-test");
        common::createFoldersIfNecessary(testDir);
        common::createFileIfNecessary(testDir / "file1");
        common::createFileIfNecessary(testDir / "file2");
        common::createFileIfNecessary(testDir / "file3");
        common::createFileIfNecessary(testDir / "file4");
        CHECK_EQUAL(common::countFilesInDirectory(testDir), 4);

        boost::filesystem::remove(testDir / "file1");
        boost::filesystem::remove(testDir / "file4");
        CHECK_EQUAL(common::countFilesInDirectory(testDir), 2);

        boost::filesystem::remove_all(testDir);
    }
    // non-existing directory
    {
        CHECK_THROWS(common::Error, common::countFilesInDirectory("/tmp/" + common::generateRandomString(16)));
    }
    // non-directory argument
    {
        auto testFile = common::PathRAII{"/tmp/file-count-test.txt"};
        common::createFileIfNecessary(testFile.getPath());
        CHECK_THROWS(common::Error, common::countFilesInDirectory(testFile.getPath()));
    }
}

TEST(UtilityTestGroup, parseMap) {
    // empty list
    {
        auto map = common::parseMap("");
        CHECK(map.empty());
    }
    // one key-value pair
    {
        auto list = "key0=value0";
        auto map = common::parseMap(list);
        CHECK_EQUAL(map.size(), 1);
        CHECK_EQUAL(map["key0"], std::string{"value0"});
    }
    // two key-value pairs
    {
        auto list = "key0=value0,key1=value1";
        auto map = common::parseMap(list);
        CHECK_EQUAL(map.size(), 2);
        CHECK_EQUAL(map["key0"], std::string{"value0"});
        CHECK_EQUAL(map["key1"], std::string{"value1"});
    }
    // key only (no value associated)
    {
        auto list = "key_only";
        auto map = common::parseMap(list);
        CHECK_EQUAL(map.size(), 1);
        CHECK(map["key_only"] == "");
    }
    {
        auto list = "key_only_at_begin,key=value";
        auto map = common::parseMap(list);
        CHECK_EQUAL(map.size(), 2);
        CHECK(map["key_only_at_begin"] == "");
        CHECK(map["key"] == "value");
    }
    {
        auto list = "key=value,key_only_at_end";
        auto map = common::parseMap(list);
        CHECK_EQUAL(map.size(), 2);
        CHECK(map["key"] == "value");
        CHECK(map["key_only_at_end"] == "");
    }
    {
        auto list = "key_only0,key_only1";
        auto map = common::parseMap(list);
        CHECK_EQUAL(map.size(), 2);
        CHECK(map["key_only0"] == "");
        CHECK(map["key_only1"] == "");
    }
    // missing key error
    {
        auto list = ",key=value";
        CHECK_THROWS(common::Error, common::parseMap(list));
    }
    {
        auto list = "key0=value0,,key1=value1";
        CHECK_THROWS(common::Error, common::parseMap(list));
    }
    {
        auto list = "key0=value0,";
        CHECK_THROWS(common::Error, common::parseMap(list));
    }
    // repeated key error
    {
        auto list = "key0=value0,key0=value1";
        CHECK_THROWS(common::Error, common::parseMap(list));
    }
    // too many values error
    {
        auto list = "key0=value0=value1";
        CHECK_THROWS(common::Error, common::parseMap(list));
    }
}

TEST(UtilityTestGroup, realpathWithinRootfs) {
    auto path = common::PathRAII{common::makeUniquePathWithRandomSuffix("/tmp/sarus-rootfs")};
    const auto& rootfs = path.getPath();

    common::createFoldersIfNecessary(rootfs / "dir0/dir1");
    common::createFoldersIfNecessary(rootfs / "dirX");
    common::createFileIfNecessary(rootfs / "dir0/dir1/file");

    // folder
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1") == "/dir0/dir1");

    // file
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/file") == "/dir0/dir1/file");

    // relative symlink
    CHECK_EQUAL(symlink("../../dir0/dir1", (rootfs / "dir0/dir1/link_relative").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_relative") == "/dir0/dir1");

    // relative symlink that spills (out of rootfs)
    CHECK_EQUAL(symlink("../../../../dir0/dir1", (rootfs / "dir0/dir1/link_relative_that_spills").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_relative_that_spills") == "/dir0/dir1");

    // relative symlink recursive
    CHECK_EQUAL(symlink("../../dir0/dir1/link_relative/dir2/dir3", (rootfs / "dir0/dir1/link_relative_recursive").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_relative_recursive") == "/dir0/dir1/dir2/dir3");

    // relative symlink recursive that spills (out of rootfs)
    CHECK_EQUAL(symlink("../../../dir0/dir1/link_relative_that_spills/dir2/dir3", (rootfs / "dir0/dir1/link_relative_recursive_that_spills").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_relative_recursive_that_spills") == "/dir0/dir1/dir2/dir3");

    // absolute symlink
    CHECK_EQUAL(symlink("/dir0/dir1", (rootfs / "dir0/dir1/link_absolute").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_absolute") == "/dir0/dir1");

    // absolute symlink that spills (out of rootfs)
    CHECK_EQUAL(symlink("/dir0/dir1/../../../../dir0/dir1", (rootfs / "dir0/dir1/link_absolute_that_spills").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_absolute_that_spills") == "/dir0/dir1");

    // absolute symlink recursive
    CHECK_EQUAL(symlink("/dir0/dir1/link_absolute/dir2/dir3", (rootfs / "dir0/dir1/link_absolute_recursive").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_absolute_recursive") == "/dir0/dir1/dir2/dir3");

    // absolute symlink recursive that spills (out of rootfs)
    CHECK_EQUAL(symlink("/dir0/dir1/link_absolute_that_spills/dir2/dir3", (rootfs / "dir0/dir1/link_absolute_recursive_that_spills").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_absolute_recursive_that_spills") == "/dir0/dir1/dir2/dir3");

    // absolute symlink sharing no part of the path with the target
    CHECK_EQUAL(symlink("/dir0/dir1", (rootfs / "dirX/link_absolute_with_no_common_path").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dirX/link_absolute_with_no_common_path") == "/dir0/dir1");
}

TEST(UtilityTestGroup, getSharedLibLinkerName) {
    CHECK(common::getSharedLibLinkerName("file.so") == "file.so");
    CHECK(common::getSharedLibLinkerName("file.so.1") == "file.so");
    CHECK(common::getSharedLibLinkerName("file.so.1.0") == "file.so");
    CHECK(common::getSharedLibLinkerName("file.so.1.0.0") == "file.so");

    CHECK_THROWS(common::Error, common::getSharedLibLinkerName("not-a-shared-lib"));
    CHECK_THROWS(common::Error, common::getSharedLibLinkerName("not-a-shared-lib.soa"));
}

TEST(UtilityTestGroup, isSharedLib) {
    CHECK(common::isSharedLib("/dir/libc.so") == true);
    CHECK(common::isSharedLib("libc.so") == true);
    CHECK(common::isSharedLib("libc.so.1") == true);
    CHECK(common::isSharedLib("libc.so.1.2") == true);

    CHECK(common::isSharedLib("libc") == false);
    CHECK(common::isSharedLib("libc.s") == false);
    CHECK(common::isSharedLib("ld.so.conf") == false);
    CHECK(common::isSharedLib("ld.so.cache") == false);
}

TEST(UtilityTestGroup, parseSharedLibAbi) {
    CHECK_THROWS(common::Error, common::parseSharedLibAbi("invalid"));
    CHECK(common::parseSharedLibAbi("libc.so") == (std::vector<std::string>{}));
    CHECK(common::parseSharedLibAbi("libc.so.1") == (std::vector<std::string>{"1"}));
    CHECK(common::parseSharedLibAbi("libc.so.1.2") == (std::vector<std::string>{"1", "2"}));
    CHECK(common::parseSharedLibAbi("libc.so.1.2.3") == (std::vector<std::string>{"1", "2", "3"}));
    CHECK(common::parseSharedLibAbi("libc.so.1.2.3rc1") == (std::vector<std::string>{"1", "2", "3rc1"}));

    CHECK(common::parseSharedLibAbi("libfoo.so.0") == (std::vector<std::string>{"0"}));
}

TEST(UtilityTestGroup, resolveSharedLibAbi) {
    auto testDirRaii = common::PathRAII{
        common::makeUniquePathWithRandomSuffix("/tmp/sarus-test-utility-resolveSharedLibAbi")
    };
    const auto& testDir = testDirRaii.getPath();

    // invalid library filename
    common::createFileIfNecessary(testDir / "invalid");
    CHECK_THROWS(common::Error, common::resolveSharedLibAbi(testDir / "invalid"));

    // libtest.so
    common::createFileIfNecessary(testDir / "libtest.so");
    CHECK(common::resolveSharedLibAbi(testDir / "libtest.so") == std::vector<std::string>{});

    // libtest.so.1
    common::createFileIfNecessary(testDir / "libtest.so.1");
    CHECK(common::resolveSharedLibAbi(testDir / "libtest.so.1") == std::vector<std::string>{"1"});

    // libtest_symlink.so.1 -> libtest_symlink.so.1.2
    common::createFileIfNecessary(testDir / "libtest_symlink.so.1.2");
    boost::filesystem::create_symlink(testDir / "libtest_symlink.so.1.2", testDir / "libtest_symlink.so.1");
    CHECK(common::resolveSharedLibAbi(testDir / "libtest_symlink.so.1") == (std::vector<std::string>{"1", "2"}));

    // libtest_symlink.so.1.2.3 -> libtest_symlink.so.1.2
    boost::filesystem::create_symlink(testDir / "libtest_symlink.so.1.2", testDir / "libtest_symlink.so.1.2.3");
    CHECK(common::resolveSharedLibAbi(testDir / "libtest_symlink.so.1.2.3") == (std::vector<std::string>{"1", "2", "3"}));

    // libtest_symlink.so -> libtest_symlink.so.1.2.3 -> libtest_symlink.so.1.2
    boost::filesystem::create_symlink(testDir / "libtest_symlink.so.1.2.3", testDir / "libtest_symlink.so");
    CHECK(common::resolveSharedLibAbi(testDir / "libtest_symlink.so") == (std::vector<std::string>{"1", "2", "3"}));

    // subdir/libtest_symlink.so -> ../libtest_symlink.so.1.2.3 -> libtest_symlink.so.1.2
    common::createFoldersIfNecessary(testDir / "subdir");
    boost::filesystem::create_symlink("../libtest_symlink.so.1.2.3", testDir / "subdir/libtest_symlink.so");
    CHECK(common::resolveSharedLibAbi(testDir / "subdir/libtest_symlink.so") == (std::vector<std::string>{"1", "2", "3"}));

    // /libtest_symlink_within_rootdir.so -> /subdir/libtest_symlink_within_rootdir.so.1 -> ../libtest_symlink_within_rootdir.so.1.2
    boost::filesystem::create_symlink("/subdir/libtest_symlink_within_rootdir.so.1", testDir / "libtest_symlink_within_rootdir.so");
    boost::filesystem::create_symlink("../libtest_symlink_within_rootdir.so.1.2", testDir / "/subdir/libtest_symlink_within_rootdir.so.1");
    common::createFileIfNecessary(testDir / "libtest_symlink_within_rootdir.so.1.2");
    CHECK(common::resolveSharedLibAbi("/libtest_symlink_within_rootdir.so", testDir) == (std::vector<std::string>{"1", "2"}));

    // Some vendors have symlinks with incompatible major versions,
    // like libvdpau_nvidia.so.1 -> libvdpau_nvidia.so.440.33.01.
    // For these cases, we trust the vendor and resolve the Lib Abi to that of the symlink.
    // Note here we use libtest.so.1 as the "original lib file" and create a symlink to it.
    boost::filesystem::create_symlink(testDir / "libtest.so.1", testDir / "libtest.so.234.56");
    CHECK(common::resolveSharedLibAbi(testDir / "libtest.so.234.56") == (std::vector<std::string>{"234", "56"}));

    boost::filesystem::create_symlink("../libtest.so.1.2", testDir / "subdir" / "libtest.so.234.56");
    CHECK(common::resolveSharedLibAbi(testDir / "subdir" / "libtest.so.234.56") == (std::vector<std::string>{"234", "56"}));

    boost::filesystem::create_symlink("../libtest.so.1.2", testDir / "subdir" / "libtest.so.234");
    CHECK(common::resolveSharedLibAbi(testDir / "subdir" / "libtest.so.234") == (std::vector<std::string>{"234"}));
}

TEST(UtilityTestGroup, getSharedLibSoname) {
    auto dummyLibsDir = boost::filesystem::path{__FILE__}
        .parent_path()
        .parent_path()
        .parent_path()
        .parent_path() / "CI/dummy_libs";
    CHECK_EQUAL(common::getSharedLibSoname(dummyLibsDir / "libc.so.6-host", "readelf"), std::string("libc.so.6"));
    CHECK_EQUAL(common::getSharedLibSoname(dummyLibsDir / "ld-linux-x86-64.so.2-host", "readelf"), std::string("ld-linux-x86-64.so.2"));
    CHECK_THROWS(common::Error, common::getSharedLibSoname(dummyLibsDir / "lib_dummy_0.so", "readelf"));
}

TEST(UtilityTestGroup, isLibc) {
    // libc
    CHECK(common::isLibc("libc.so"));
    CHECK(common::isLibc("libc.so.6"));
    CHECK(common::isLibc("libc-2.29.so"));
    CHECK(common::isLibc("/libc.so"));
    CHECK(common::isLibc("../libc.so"));
    CHECK(common::isLibc("dir/libc.so"));
    CHECK(common::isLibc("dir/dir/libc.so"));
    CHECK(common::isLibc("/root/libc.so"));
    CHECK(common::isLibc("/root/dir/libc.so"));

    // not libc
    CHECK(!common::isLibc("libcl.so"));
    CHECK(!common::isLibc("libc_bogus.so"));
}

TEST(UtilityTestGroup, parseLibcVersion) {
    CHECK((std::tuple<unsigned int, unsigned int>{0, 0} == common::parseLibcVersion("libc-0.0.so")));
    CHECK((std::tuple<unsigned int, unsigned int>{2, 29} == common::parseLibcVersion("libc-2.29.so")));
    CHECK((std::tuple<unsigned int, unsigned int>{100, 100} == common::parseLibcVersion("libc-100.100.so")));
}

TEST(UtilityTestGroup, is64bitSharedLib) {
    auto dummyLibsDir = boost::filesystem::path{__FILE__}
        .parent_path()
        .parent_path()
        .parent_path()
        .parent_path() / "CI/dummy_libs";
    CHECK(common::is64bitSharedLib(dummyLibsDir / "libc.so.6-host", "readelf"));
    CHECK(common::is64bitSharedLib(dummyLibsDir / "ld-linux-x86-64.so.2-host", "readelf"));
    CHECK(!common::is64bitSharedLib(dummyLibsDir / "libc.so.6-32bit-container", "readelf"));
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

    auto actual = common::serializeJSON(json);
    auto expected = std::string{"{\"string\":\"stringValue\",\"int\":11,\"array\":[0,1,2]}"};

    CHECK_EQUAL(common::removeWhitespaces(actual), expected);
}

TEST(UtilityTestGroup, convertCppRestJsonToRapidJson) {
    auto cppRest = web::json::value{};
    cppRest[U("object")] = web::json::value{};
    cppRest[U("object")][U("string")] = web::json::value("text");
    cppRest[U("object")][U("list")][0] = web::json::value("element0");
    cppRest[U("object")][U("list")][1] = web::json::value("element1");
    cppRest[U("object")][U("subobject")] = web::json::value{};
    cppRest[U("object")][U("subobject")][U("string")] = web::json::value("text");

    auto rapidJson = common::convertCppRestJsonToRapidJson(cppRest);

    CHECK_EQUAL(rapidJson["object"]["string"].GetString(), std::string{"text"});
    CHECK_EQUAL(rapidJson["object"]["list"].Size(), 2);
    CHECK_EQUAL(rapidJson["object"]["list"].GetArray()[0].GetString(), std::string{"element0"});
    CHECK_EQUAL(rapidJson["object"]["list"].GetArray()[1].GetString(), std::string{"element1"});
    CHECK_EQUAL(rapidJson["object"]["subobject"]["string"].GetString(), std::string{"text"});
}

TEST(UtilityTestGroup, setCpuAffinity_invalid_argument) {
    CHECK_THROWS(common::Error, common::setCpuAffinity({})); // no CPUs
}

TEST(UtilityTestGroup, getCpuAffinity_setCpuAffinity) {
    auto initialCpus = common::getCpuAffinity();

    if(initialCpus.size() <= 1) {
        std::cerr << "Skipping CPU affinity unit test. Not enough CPUs available" << std::endl;
        return;
    }

    // set new affinity (removing one CPU)
    auto newCpus = initialCpus;
    newCpus.pop_back();
    common::setCpuAffinity(newCpus);

    // check
    CHECK(common::getCpuAffinity() == newCpus);

    // restore initial affinity
    common::setCpuAffinity(initialCpus);
}

SARUS_UNITTEST_MAIN_FUNCTION();
