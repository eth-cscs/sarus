#include <fstream>
#include <streambuf>
#include <boost/filesystem.hpp>

#include "hooks/ssh/PasswdDB.hpp"
#include "test_utility/unittest_main_function.hpp"


namespace sarus {
namespace hooks {
namespace ssh {
namespace test {

TEST_GROUP(PasswdDBTestGroup) {
};

TEST(PasswdDBTestGroup, testRead) {
    // create file
    auto file = boost::filesystem::path{"/tmp/test-passwd-file"};
    std::ofstream of{file.c_str()};
    of  << "loginName0:x:1000:1001:UserNameOrCommentField0:/home/dir0"
        << std::endl
        << "loginName1:encryptedPassword1:2000:2001:UserNameOrCommentField1:/home/dir1:/optional/UserCommandInterpreter1"
        << std::endl;

    // read from file
    auto passwd = PasswdDB{};
    passwd.read(file);
    const auto& entries = passwd.getEntries();

    CHECK(entries.size() == 2);

    CHECK(entries[0].loginName == "loginName0");
    CHECK(entries[0].encryptedPassword == "x");
    CHECK(entries[0].uid == 1000);
    CHECK(entries[0].gid == 1001);
    CHECK(entries[0].userNameOrCommentField == "UserNameOrCommentField0");
    CHECK(entries[0].userHomeDirectory == "/home/dir0");
    CHECK(!entries[0].userCommandInterpreter);

    CHECK(entries[1].loginName == "loginName1");
    CHECK(entries[1].encryptedPassword == "encryptedPassword1");
    CHECK(entries[1].uid == 2000);
    CHECK(entries[1].gid == 2001);
    CHECK(entries[1].userNameOrCommentField == "UserNameOrCommentField1");
    CHECK(entries[1].userHomeDirectory == "/home/dir1");
    CHECK(*entries[1].userCommandInterpreter == "/optional/UserCommandInterpreter1");

    boost::filesystem::remove_all(file);
}

TEST(PasswdDBTestGroup, testWrite) {
    auto file = boost::filesystem::path{"/tmp/test-passwd-file"};

    // create entry
    auto entry0 = PasswdDB::Entry{
        "loginName0",
        "x",
        1000,
        1001,
        "UserNameOrCommentField0",
        "/home/dir0",
        boost::filesystem::path{"/optional/UserCommandInterpreter0"}
    };
    auto entry1 = PasswdDB::Entry {
        "loginName1",
        "y",
        2000,
        2001,
        "UserNameOrCommentField1",
        "/home/dir1",
        {}
    };
    auto passwd = PasswdDB{};
    passwd.getEntries() = {entry0, entry1};

    // write to file
    passwd.write(file);

    // check file contents
    std::ifstream is(file.c_str());
    auto data = std::string(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>());
    auto expectedData = std::string{"loginName0:x:1000:1001:UserNameOrCommentField0:/home/dir0:/optional/UserCommandInterpreter0\n"
                                    "loginName1:y:2000:2001:UserNameOrCommentField1:/home/dir1:\n"};
    CHECK_EQUAL(data, expectedData);

    boost::filesystem::remove_all(file);
}

}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
