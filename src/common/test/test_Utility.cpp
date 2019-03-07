#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>

#include "common/Utility.hpp"
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
    common::createFileIfNecessary("/tmp/src");

    common::copyFile("/tmp/src", "/tmp/dst");
    CHECK((common::getOwner("/tmp/dst") == std::tuple<uid_t, gid_t>{0, 0}));
    boost::filesystem::remove_all("/tmp/dst");

    common::copyFile("/tmp/src", "/tmp/dst", 1000, 1000);
    CHECK((common::getOwner("/tmp/dst") == std::tuple<uid_t, gid_t>{1000, 1000}));
    boost::filesystem::remove_all("/tmp/src");
    boost::filesystem::remove_all("/tmp/dst");
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

TEST(UtilityTestGroup, convertListOfKeyValuePairsToMap) {
    // empty list
    {
        auto map = common::convertListOfKeyValuePairsToMap("");
        CHECK(map.empty());
    }
    // one key-value pair
    {
        auto list = "key0=value0";
        auto map = common::convertListOfKeyValuePairsToMap(list);
        CHECK_EQUAL(map.size(), 1);
        CHECK_EQUAL(map["key0"], std::string{"value0"});
    }
    // two key-value pairs
    {
        auto list = "key0=value0,key1=value1";
        auto map = common::convertListOfKeyValuePairsToMap(list);
        CHECK_EQUAL(map.size(), 2);
        CHECK_EQUAL(map["key0"], std::string{"value0"});
        CHECK_EQUAL(map["key1"], std::string{"value1"});
    }
    // key only (no value associated)
    {
        auto list = "key_only";
        auto map = common::convertListOfKeyValuePairsToMap(list);
        CHECK_EQUAL(map.size(), 1);
        CHECK(map["key_only"] == "");
    }
    {
        auto list = "key_only_at_begin,key=value";
        auto map = common::convertListOfKeyValuePairsToMap(list);
        CHECK_EQUAL(map.size(), 2);
        CHECK(map["key_only_at_begin"] == "");
        CHECK(map["key"] == "value");
    }
    {
        auto list = "key=value,key_only_at_end";
        auto map = common::convertListOfKeyValuePairsToMap(list);
        CHECK_EQUAL(map.size(), 2);
        CHECK(map["key"] == "value");
        CHECK(map["key_only_at_end"] == "");
    }
    {
        auto list = "key_only0,key_only1";
        auto map = common::convertListOfKeyValuePairsToMap(list);
        CHECK_EQUAL(map.size(), 2);
        CHECK(map["key_only0"] == "");
        CHECK(map["key_only1"] == "");
    }
    // missing value error
    {
        auto list = "key=";
        CHECK_THROWS(common::Error, common::convertListOfKeyValuePairsToMap(list));
    }
    {
        auto list = "key0,key1=,key2";
        CHECK_THROWS(common::Error, common::convertListOfKeyValuePairsToMap(list));
    }
    // missing key error
    {
        auto list = ",key=value";
        CHECK_THROWS(common::Error, common::convertListOfKeyValuePairsToMap(list));
    }
    {
        auto list = "key0=value0,,key1=value1";
        CHECK_THROWS(common::Error, common::convertListOfKeyValuePairsToMap(list));
    }
    {
        auto list = "key0=value0,";
        CHECK_THROWS(common::Error, common::convertListOfKeyValuePairsToMap(list));
    }
    // repeated key error
    {
        auto list = "key0=value0,key0=value1";
        CHECK_THROWS(common::Error, common::convertListOfKeyValuePairsToMap(list));
    }
}

TEST(UtilityTestGroup, convertStringListToVector) {
    // no entries
    auto vec = common::convertStringListToVector("");
    CHECK(vec.empty());

    vec = common::convertStringListToVector(";");
    CHECK(vec.empty());

    vec = common::convertStringListToVector(";;");
    CHECK(vec.empty());

    // two entries
    vec = common::convertStringListToVector("one;two");
    CHECK(vec == (std::vector<std::string>{"one", "two"}));

    vec = common::convertStringListToVector(";one;two;");
    CHECK(vec == (std::vector<std::string>{"one", "two"}));

    vec = common::convertStringListToVector(";;one;;two;;");
    CHECK(vec == (std::vector<std::string>{"one", "two"}));

    // three entries
    vec = common::convertStringListToVector("one;two;three");
    CHECK(vec == (std::vector<std::string>{"one", "two", "three"}));
}

TEST(UtilityTestGroup, realpathWithinRootfs) {
    auto rootfs = common::makeUniquePathWithRandomSuffix("/tmp/sarus-rootfs");

    common::createFoldersIfNecessary(rootfs / "dir0/dir1");
    common::createFoldersIfNecessary(rootfs / "dirX");
    common::createFileIfNecessary(rootfs / "dir0/dir1/file");

    // folder
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1") == (rootfs / "dir0/dir1"));

    // file
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/file") == (rootfs / "dir0/dir1/file"));

    // relative symlink
    CHECK_EQUAL(symlink("../../dir0/dir1", (rootfs / "dir0/dir1/link_relative").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_relative") == (rootfs / "dir0/dir1"));

    // relative symlink that spills (out of rootfs)
    CHECK_EQUAL(symlink("../../../../dir0/dir1", (rootfs / "dir0/dir1/link_relative_that_spills").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_relative_that_spills") == (rootfs / "dir0/dir1"));
    
    // relative symlink recursive
    CHECK_EQUAL(symlink("../../dir0/dir1/link_relative/dir2/dir3", (rootfs / "dir0/dir1/link_relative_recursive").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_relative_recursive") == (rootfs / "dir0/dir1/dir2/dir3"));

    // relative symlink recursive that spills (out of rootfs)
    CHECK_EQUAL(symlink("../../../dir0/dir1/link_relative_that_spills/dir2/dir3", (rootfs / "dir0/dir1/link_relative_recursive_that_spills").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_relative_recursive_that_spills") == (rootfs / "dir0/dir1/dir2/dir3"));

    // absolute symlink
    CHECK_EQUAL(symlink("/dir0/dir1", (rootfs / "dir0/dir1/link_absolute").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_absolute") == (rootfs / "dir0/dir1"));

    // absolute symlink that spills (out of rootfs)
    CHECK_EQUAL(symlink("/dir0/dir1/../../../../dir0/dir1", (rootfs / "dir0/dir1/link_absolute_that_spills").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_absolute_that_spills") == (rootfs / "dir0/dir1"));

    // absolute symlink recursive
    CHECK_EQUAL(symlink("/dir0/dir1/link_absolute/dir2/dir3", (rootfs / "dir0/dir1/link_absolute_recursive").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_absolute_recursive") == (rootfs / "dir0/dir1/dir2/dir3"));

    // absolute symlink recursive that spills (out of rootfs)
    CHECK_EQUAL(symlink("/dir0/dir1/link_absolute_that_spills/dir2/dir3", (rootfs / "dir0/dir1/link_absolute_recursive_that_spills").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dir0/dir1/link_absolute_recursive_that_spills") == (rootfs / "dir0/dir1/dir2/dir3"));

    // absolute symlink sharing no part of the path with the target
    CHECK_EQUAL(symlink("/dir0/dir1", (rootfs / "dirX/link_absolute_with_no_common_path").string().c_str()), 0);
    CHECK(common::realpathWithinRootfs(rootfs, "/dirX/link_absolute_with_no_common_path") == (rootfs / "dir0/dir1"));

    boost::filesystem::remove_all(rootfs);
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

SARUS_UNITTEST_MAIN_FUNCTION();
