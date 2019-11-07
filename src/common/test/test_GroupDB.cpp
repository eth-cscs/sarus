/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <fstream>
#include <streambuf>
#include <boost/filesystem.hpp>

#include "common/Utility.hpp"
#include "common/GroupDB.hpp"
#include "test_utility/unittest_main_function.hpp"


namespace sarus {
namespace common {
namespace test {

TEST_GROUP(GroupDBTestGroup) {
};

TEST(GroupDBTestGroup, testRead) {
    // create file
    auto file = common::makeUniquePathWithRandomSuffix("./test-etc-group-file");
    std::ofstream of{file.c_str()};
    of  << "groupName0:x:0:" << std::endl
        << "groupName1:x:1:userName0" << std::endl
        << "groupName2:x:2:userName0,userName1" << std::endl;

    // read from file
    auto group = GroupDB{};
    group.read(file);
    const auto& entries = group.getEntries();

    CHECK(entries.size() == 3);

    CHECK(entries[0].groupName == "groupName0");
    CHECK(entries[0].encryptedPassword == "x");
    CHECK(entries[0].gid == 0);
    CHECK(entries[0].users == std::vector<std::string>{});

    CHECK(entries[1].groupName == "groupName1");
    CHECK(entries[1].encryptedPassword == "x");
    CHECK(entries[1].gid == 1);
    CHECK(entries[1].users == std::vector<std::string>{"userName0"});

    CHECK(entries[2].groupName == "groupName2");
    CHECK(entries[2].encryptedPassword == "x");
    CHECK(entries[2].gid == 2);
    CHECK(entries[2].users == (std::vector<std::string>{"userName0", "userName1"}));

    boost::filesystem::remove_all(file);
}

TEST(GroupDBTestGroup, testWrite) {
    auto file = common::makeUniquePathWithRandomSuffix("./test-etc-group-file");

    // create entry
    auto entry0 = GroupDB::Entry{
        "groupName0",
        "x",
        0,
        {}
    };
    auto entry1 = GroupDB::Entry {
        "groupName1",
        "y",
        1,
        {"userName0"}
    };
    auto entry2 = GroupDB::Entry {
        "groupName2",
        "z",
        2,
        {"userName0", "userName1"}
    };
    auto group = GroupDB{};
    group.getEntries() = {entry0, entry1, entry2};

    // write to file
    group.write(file);

    // check file contents
    std::ifstream is(file.c_str());
    auto data = std::string(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>());
    auto expectedData = std::string{"groupName0:x:0:\n"
                                    "groupName1:y:1:userName0\n"
                                    "groupName2:z:2:userName0,userName1\n"};
    CHECK_EQUAL(data, expectedData);

    boost::filesystem::remove_all(file);
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
