/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <boost/predef.h>
#include <rapidjson/document.h>

#include "libsarus/Utility.hpp"
#include "image_manager/Utility.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace rj = rapidjson;

namespace sarus {
namespace image_manager {
namespace test {

TEST_GROUP(ImageManagerUtilityTestGroup) {
};

#if BOOST_ARCH_X86_64
TEST(ImageManagerUtilityTestGroup, getCurrentOCIPlatform) {
#else
IGNORE_TEST(ImageManagerUtilityTestGroup, getCurrentOCIPlatform) {
#endif
    auto currentPlatform = utility::getCurrentOCIPlatform();

    auto expectedPlatform = rj::Document{rj::kObjectType};
    auto allocator = expectedPlatform.GetAllocator();
    expectedPlatform.AddMember("os", rj::Value{"linux"}, allocator);
    expectedPlatform.AddMember("architecture", rj::Value{"amd64", allocator}, allocator);
    expectedPlatform.AddMember("variant", rj::Value{""}, allocator);

    CHECK(currentPlatform == expectedPlatform);
}

TEST(ImageManagerUtilityTestGroup, getPlatformDigestFromOCIIndex) {
    auto platform = rj::Document{rj::kObjectType};
    auto allocator = platform.GetAllocator();
    platform.AddMember("os", rj::Value{"linux"}, allocator);
    platform.AddMember("architecture", rj::Value{"", allocator}, allocator);
    platform.AddMember("variant", rj::Value{""}, allocator);

    // Alpine manifest list
    {
        auto manifestListPath = boost::filesystem::path{__FILE__}.parent_path() / "docker_manifest_list_alpine.json";
        auto manifestList = libsarus::readJSON(manifestListPath);

        platform["architecture"] = "amd64";
        auto returnedDigest = utility::getPlatformDigestFromOCIIndex(manifestList, platform);
        auto expectedDigest = std::string{"sha256:e7d88de73db3d3fd9b2d63aa7f447a10fd0220b7cbf39803c803f2af9ba256b3"};
        CHECK_EQUAL(returnedDigest, expectedDigest);

        platform["architecture"] = "386";
        returnedDigest = utility::getPlatformDigestFromOCIIndex(manifestList, platform);
        expectedDigest = std::string{"sha256:2689e157117d2da668ad4699549e55eba1ceb79cb7862368b30919f0488213f4"};
        CHECK_EQUAL(returnedDigest, expectedDigest);

        platform["architecture"] = "ppc64le";
        returnedDigest = utility::getPlatformDigestFromOCIIndex(manifestList, platform);
        expectedDigest = std::string{"sha256:2042a492bcdd847a01cd7f119cd48caa180da696ed2aedd085001a78664407d6"};
        CHECK_EQUAL(returnedDigest, expectedDigest);

        platform["architecture"] = "s390x";
        returnedDigest = utility::getPlatformDigestFromOCIIndex(manifestList, platform);
        expectedDigest = std::string{"sha256:49e322ab6690e73a4909f787bcbdb873631264ff4a108cddfd9f9c249ba1d58e"};
        CHECK_EQUAL(returnedDigest, expectedDigest);

        platform["architecture"] = "arm64";
        platform["variant"] = "v8";
        returnedDigest = utility::getPlatformDigestFromOCIIndex(manifestList, platform);
        expectedDigest = std::string{"sha256:c74f1b1166784193ea6c8f9440263b9be6cae07dfe35e32a5df7a31358ac2060"};
        CHECK_EQUAL(returnedDigest, expectedDigest);
    }
    // Debian manifest list
    {
        auto manifestListPath = boost::filesystem::path{__FILE__}.parent_path() / "docker_manifest_list_debian.json";
        auto manifestList = libsarus::readJSON(manifestListPath);

        platform["architecture"] = "amd64";
        auto returnedDigest = utility::getPlatformDigestFromOCIIndex(manifestList, platform);
        auto expectedDigest = std::string{"sha256:7d8264bf731fec57d807d1918bec0a16550f52a9766f0034b40f55c5b7dc3712"};
        CHECK_EQUAL(returnedDigest, expectedDigest);

        platform["architecture"] = "mips64le";
        returnedDigest = utility::getPlatformDigestFromOCIIndex(manifestList, platform);
        expectedDigest = std::string{"sha256:04ca681ba051d44288c14f25c2f072d0bdf784a7963bc0a4085e9e622f9cb89e"};
        CHECK_EQUAL(returnedDigest, expectedDigest);

        platform["architecture"] = "arm64";
        platform["variant"] = "v8";
        returnedDigest = utility::getPlatformDigestFromOCIIndex(manifestList, platform);
        expectedDigest = std::string{"sha256:b3d4eb0332b522963a898e4bbac06c8129ffa0f90ae8862d25313633def3f2c2"};
        CHECK_EQUAL(returnedDigest, expectedDigest);
    }
}

TEST(ImageManagerUtilityTestGroup, base64Encode) {
    CHECK(utility::base64Encode("") == "");
    CHECK(utility::base64Encode("abc") == "YWJj");
    CHECK(utility::base64Encode("abc1") == "YWJjMQ==");
    CHECK(utility::base64Encode("ZyxWvut0987654") == "Wnl4V3Z1dDA5ODc2NTQ=");
    CHECK(utility::base64Encode("username:password") == "dXNlcm5hbWU6cGFzc3dvcmQ=");
    CHECK(utility::base64Encode("alice:Aw3s0m&_P@s5w0rD") == "YWxpY2U6QXczczBtJl9QQHM1dzByRA==");
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
