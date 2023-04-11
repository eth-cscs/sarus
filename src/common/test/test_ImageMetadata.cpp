/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
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
    writtenMetadata.labels = std::unordered_map<std::string, std::string>{
        {"labelKey0", "labelValue0"},
        {"labelKey1", "labelValue1"}
    };

    auto file = common::makeUniquePathWithRandomSuffix("/tmp/sarus-test-imagemetadata");
    writtenMetadata.write(file);
    auto readMetadata = common::ImageMetadata{file, common::UserIdentity{}};

    CHECK(readMetadata == writtenMetadata);

    boost::filesystem::remove(file);
}

TEST(ImageMetadataTestGroup, read_from_json) {
    namespace rj = rapidjson;

    auto json = rj::Document(rj::kObjectType);
    auto& allocator = json.GetAllocator();

    json.AddMember("Cmd", rj::Value{rj::kArrayType}, allocator);
    json["Cmd"].PushBack("cmd", allocator);
    json["Cmd"].PushBack("arg", allocator);

    json.AddMember("Entrypoint", rj::Value{rj::kArrayType}, allocator);
    json["Entrypoint"].PushBack("entry", allocator);
    json["Entrypoint"].PushBack("arg", allocator);

    json.AddMember("WorkingDir", "/WorkingDir", allocator);

    json.AddMember("Env", rj::Value{rj::kArrayType}, allocator);
    json["Env"].PushBack("KEY0=VALUE0", allocator);
    json["Env"].PushBack("KEY1=VALUE1", allocator);

    json.AddMember("Labels", rj::Value{rj::kObjectType}, allocator);
    json["Labels"].AddMember("com.test.label.key0", "value0", allocator);
    json["Labels"].AddMember("com.test.label.key1", "value1", allocator);

    auto metadata = common::ImageMetadata{json};

    CHECK((*metadata.cmd == common::CLIArguments{"cmd", "arg"}));
    CHECK((*metadata.entry == common::CLIArguments{"entry", "arg"}));
    CHECK(*metadata.workdir == "/WorkingDir");
    auto expectedEnv = std::unordered_map<std::string, std::string>{
        {"KEY0", "VALUE0"},
        {"KEY1", "VALUE1"}
    };
    CHECK(metadata.env == expectedEnv);
    auto expectedLabels = std::unordered_map<std::string, std::string>{
        {"com.test.label.key0", "value0"},
        {"com.test.label.key1", "value1"}
    };
    CHECK(metadata.labels == expectedLabels);
}

} // namespace
} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
