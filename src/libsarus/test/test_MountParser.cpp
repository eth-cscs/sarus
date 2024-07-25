/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <rapidjson/allocators.h>
#include <rapidjson/document.h>

#include "aux/unitTestMain.hpp"
#include "libsarus/Logger.hpp"
#include "libsarus/MountParser.hpp"
#include "libsarus/PathRAII.hpp"
#include "libsarus/Utility.hpp"


namespace libsarus {
namespace test {

class MountParserChecker {
public:
    MountParserChecker(const std::string& mountRequest)
        : mountRequest(mountRequest)
    {}

    MountParserChecker& parseAsSiteMount() {
        this->isSiteMount = true;
        return *this;
    }

    MountParserChecker& expectSource(const std::string& expectedSource) {
        this->expectedSource = expectedSource;
        return *this;
    }

    MountParserChecker& expectDestination(const std::string& expectedDestination) {
        this->expectedDestination = expectedDestination;
        return *this;
    }

    MountParserChecker& expectFlags(const size_t flags) {
        this->expectedFlags = flags;
        return *this;
    }

    MountParserChecker& expectParseError() {
        isParseErrorExpected = true;
        return *this;
    }

    ~MountParserChecker() {
        libsarus::UserIdentity userIdentity;
        auto bundleDirRAII = libsarus::PathRAII{libsarus::filesystem::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("test-bundle-dir"))};
        auto rootfsDir = bundleDirRAII.getPath() / "rootfs";

        libsarus::MountParser parser = libsarus::MountParser{rootfsDir, userIdentity};

        if (!isSiteMount) {
          // Populate "userMount", originally done by "ConfigRAII" in Sarus 1.6.4.
          rapidjson::MemoryPoolAllocator<> allocator;
          rapidjson::Value userMountsValue(rapidjson::kObjectType);

          rapidjson::Value disallowWithPrefixValue(rapidjson::kArrayType);
          disallowWithPrefixValue.PushBack("/etc", allocator);
          disallowWithPrefixValue.PushBack("/var", allocator);
          disallowWithPrefixValue.PushBack("/opt/sarus", allocator);
          userMountsValue.AddMember("notAllowedPrefixesOfPath", disallowWithPrefixValue, allocator);

          rapidjson::Value disallowExactValue(rapidjson::kArrayType);
          disallowExactValue.PushBack("/opt", allocator);
          userMountsValue.AddMember("notAllowedPaths", disallowExactValue, allocator);

          // Inject "userMount".
          parser.setMountDestinationRestrictions(userMountsValue);
        }

        auto map = libsarus::string::parseMap(mountRequest);

        if(isParseErrorExpected) {
            CHECK_THROWS(libsarus::Error, parser.parseMountRequest(map));
            return;
        }

        auto mountObject = parser.parseMountRequest(map);

        if(expectedSource) {
            CHECK(mountObject->getSource() == *expectedSource);
        }

        if(expectedDestination) {
            CHECK(mountObject->getDestination() == *expectedDestination);
        }

        if(expectedFlags) {
            CHECK_EQUAL(mountObject->getFlags(), *expectedFlags);
        }
    }

private:
    std::string mountRequest;
    bool isSiteMount = false;
    boost::optional<std::string> expectedSource = {};
    boost::optional<std::string> expectedDestination = {};
    boost::optional<size_t> expectedFlags = {};

    bool isParseErrorExpected = false;
};

TEST_GROUP(MountParserTestGroup) {
};

TEST(MountParserTestGroup, mount_type) {
    // bind
    MountParserChecker{"type=bind,source=/src,destination=/dest"};

    // invalid mount type
    MountParserChecker{"type=invalid,source=/src,destination=/dest"}.expectParseError();

    // invalid mount keys
    MountParserChecker{"type=invalid,spicysouce=/src,destination=/dest"}.expectParseError();
    MountParserChecker{"type=invalid,source=/src,nation=/dest"}.expectParseError();
}

TEST(MountParserTestGroup, source_and_destination_of_bind_mount) {
    MountParserChecker{"type=bind,source=/src,destination=/dest"}
        .expectSource("/src")
        .expectDestination("/dest");

    // source alias
    MountParserChecker{"type=bind,src=/src,destination=/dest"}
        .expectSource("/src")
        .expectDestination("/dest");

    // destination aliases
    MountParserChecker{"type=bind,source=/src,dst=/dest"}
        .expectSource("/src")
        .expectDestination("/dest");
    MountParserChecker{"type=bind,source=/src,target=/dest"}
        .expectSource("/src")
        .expectDestination("/dest");

    // only absolute paths allowed
    MountParserChecker{"type=bind,source=src,destination=/dest"}.expectParseError();
    MountParserChecker{"type=bind,source=/src,destination=dest"}.expectParseError();

    // missing type
    MountParserChecker{"source=src, destination=/dest"}.expectParseError();

    // missing path
    MountParserChecker{"type=bind,source=/src"}.expectParseError();
    MountParserChecker{"type=bind,destination=/dest"}.expectParseError();

    // disallowed prefixes of destination
    MountParserChecker{"type=bind,source=/src,destination=/etc"}.expectParseError();
    MountParserChecker{"type=bind,source=/src,destination=/var"}.expectParseError();
    MountParserChecker{"type=bind,source=/src,destination=/opt"}.expectParseError();

    // disallowed destinations
    MountParserChecker{"type=bind,source=/src,destination=/opt/sarus"}.expectParseError();
}

TEST(MountParserTestGroup, user_flags_of_bind_mount) {
    // no flags: defaults to recursive, private, read/write mount
    MountParserChecker{"type=bind,source=/src,destination=/dest"}
        .expectFlags(MS_REC | MS_PRIVATE);

    // readonly mount
    MountParserChecker{"type=bind,source=/src,destination=/dest,readonly"}
        .expectFlags(MS_REC | MS_RDONLY | MS_PRIVATE);

    // Since Sarus 1.4.0, bind-propagation is no longer a valid option
    MountParserChecker{"type=bind,source=/src,destination=dest,bind-propagation=slave"}.expectParseError();
    MountParserChecker{"type=bind,source=/src,destination=dest,bind-propagation=recursive"}.expectParseError();
}

TEST(MountParserTestGroup, site_flags_of_bind_mount) {
    // no flags: defaults to recursive, private, read/write mount
    MountParserChecker{"type=bind,source=/src,destination=/dest"}
        .parseAsSiteMount().expectFlags(MS_REC | MS_PRIVATE);

    // readonly mount
    MountParserChecker{"type=bind,source=/src,destination=/dest,readonly"}
        .parseAsSiteMount().expectFlags(MS_REC | MS_RDONLY | MS_PRIVATE);
}

}}

SARUS_UNITTEST_MAIN_FUNCTION();
