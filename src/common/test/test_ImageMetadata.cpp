/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <sstream>

#include "common/Error.hpp"
#include "common/Utility.hpp"
#include "common/ImageMetadata.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace test {

TEST_GROUP(ImageMetadataTestGroup) {
};

TEST(ImageMetadataTestGroup, write_read_from_file) {
    auto writtenMetadata = common::ImageMetadata{};
    writtenMetadata.cmd = common::CLIArguments{"cmd", "arg0", "arg1"};
    writtenMetadata.entry = common::CLIArguments{"entry", "arg0", "arg1"};
    writtenMetadata.workdir = "/workdir";
    writtenMetadata.env = std::unordered_map<std::string, std::string>{
        {"key0", "value0"},
        {"key1", "value1"}
    };

    auto file = common::makeUniquePathWithRandomSuffix("/tmp/sarus-test-imagemetadata");
    writtenMetadata.write(file);
    auto readMetadata = common::ImageMetadata{file};

    CHECK(readMetadata == writtenMetadata);

    boost::filesystem::remove(file);
}

TEST(ImageMetadataTestGroup, read_from_json) {
    namespace rj = rapidjson;

    auto json = rj::Document(rj::kObjectType);
    auto& allocator = json.GetAllocator();

    json.AddMember("Cmd", rj::Value{rj::kArrayType}, allocator);
    json["Cmd"].PushBack(rj::Value{"cmd"}, allocator);
    json["Cmd"].PushBack(rj::Value{"arg"}, allocator);

    json.AddMember("Entrypoint", rj::Value{rj::kArrayType}, allocator);
    json["Entrypoint"].PushBack(rj::Value{"entry"}, allocator);
    json["Entrypoint"].PushBack(rj::Value{"arg"}, allocator);

    json.AddMember("WorkingDir", rj::Value{"/WorkingDir"}, allocator);

    json.AddMember("Env", rj::Value{rj::kArrayType}, allocator);
    json["Env"].PushBack(rj::Value{"KEY0=VALUE0"}, allocator);
    json["Env"].PushBack(rj::Value{"KEY1=VALUE1"}, allocator);

    auto metadata = common::ImageMetadata{json};

    CHECK((*metadata.cmd == common::CLIArguments{"cmd", "arg"}));
    CHECK((*metadata.entry == common::CLIArguments{"entry", "arg"}));
    CHECK(*metadata.workdir == "/WorkingDir");
    auto expectedEnv = std::unordered_map<std::string, std::string>{
        {"KEY0", "VALUE0"},
        {"KEY1", "VALUE1"}
    };
    CHECK(metadata.env == expectedEnv);
}

} // namespace
} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
