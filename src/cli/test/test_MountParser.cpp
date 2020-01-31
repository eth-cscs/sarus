/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Checker.hpp"
#include "common/Logger.hpp"
#include "cli/MountParser.hpp"
#include "test_utility/unittest_main_function.hpp"


namespace sarus {
namespace cli {
namespace custom_mounts {
namespace test {

TEST_GROUP(MountParserTestGroup) {
};

TEST(MountParserTestGroup, mount_type) {
    // bind
    Checker{"type=bind,source=/src,destination=/dest"};
    // invalid mount type
    Checker{"type=invalid,source=/src,destination=/dest"}.expectParseError();
    // invalid mount keys
    Checker{"type=invalid,spicysouce=/src,destination=/dest"}.expectParseError();
    Checker{"type=invalid,source=/src,nation=/dest"}.expectParseError();
}

TEST(MountParserTestGroup, source_and_destination_of_bind_mount) {
    Checker{"type=bind,source=/src,destination=/dest"}
        .expectSource("/src")
        .expectDestination("/dest");

    // source alias
    Checker{"type=bind,src=/src,destination=/dest"}
        .expectSource("/src")
        .expectDestination("/dest");

    // destination aliases
    Checker{"type=bind,source=/src,dst=/dest"}
        .expectSource("/src")
        .expectDestination("/dest");
    Checker{"type=bind,source=/src,target=/dest"}
        .expectSource("/src")
        .expectDestination("/dest");

    // only absolute paths allowed
    Checker{"type=bind,source=src,destination=/dest"}.expectParseError();
    Checker{"type=bind,source=/src,destination=dest"}.expectParseError();

    // missing type
    Checker{"source=src, destination=/dest"}.expectParseError();

    // missing path
    Checker{"type=bind,source=/src"}.expectParseError();
    Checker{"type=bind,destination=/dest"}.expectParseError();

    // disallowed prefixes of destination
    Checker{"type=bind,source=/src,destination=/etc"}.expectParseError();
    Checker{"type=bind,source=/src,destination=/var"}.expectParseError();
    Checker{"type=bind,source=/src,destination=/opt"}.expectParseError();

    // disallowed destinations
    Checker{"type=bind,source=/src,destination=/opt/sarus"}.expectParseError();
}

TEST(MountParserTestGroup, user_flags_of_bind_mount) {
    Checker{"type=bind,source=/src,destination=/dest,readonly"}
        .expectFlags({MS_REC | MS_RDONLY | MS_PRIVATE});

    // checks for deprecated "bind-propagation" flag
    Checker{"type=bind,source=/src,destination=/dest,bind-propagation=recursive"}.expectFlags({MS_REC});
    Checker{"type=bind,source=/src,destination=/dest,bind-propagation=private"}.expectParseError();
    Checker{"type=bind,source=/src,destination=/dest,bind-propagation=rprivate"}.expectParseError();
    Checker{"type=bind,source=/src,destination=/dest,bind-propagation=slave"}.expectParseError();
    Checker{"type=bind,source=/src,destination=/dest,bind-propagation=rslave"}.expectParseError();
}

TEST(MountParserTestGroup, site_flags_of_bind_mount) {
    Checker{"type=bind,source=/src,destination=/dest,readonly"}
        .parseAsSiteMount().expectFlags(MS_REC | MS_RDONLY | MS_PRIVATE);

    // checks for deprecated "bind-propagation" flag
    Checker{"type=bind,source=/src,destination=/dest,bind-propagation=recursive"}
        .parseAsSiteMount().expectFlags(MS_REC);
    Checker{"type=bind,source=/src,destination=/dest,bind-propagation=private"}
        .parseAsSiteMount().expectFlags(MS_PRIVATE);
    Checker{"type=bind,source=/src,destination=/dest,bind-propagation=slave"}
        .parseAsSiteMount().expectFlags(MS_SLAVE);
    Checker{"type=bind,source=/src,destination=/dest,bind-propagation=rprivate"}
        .parseAsSiteMount().expectFlags(MS_REC | MS_PRIVATE);
    Checker{"type=bind,source=/src,destination=/dest,bind-propagation=rslave"}
        .parseAsSiteMount().expectFlags(MS_REC | MS_SLAVE);
}

} // namespace
} // namespace
} // namespace
} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
