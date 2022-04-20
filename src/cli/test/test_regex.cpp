/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <iostream>
#include "cli/regex.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace cli {
namespace test {

TEST_GROUP(CLIRegexTestGroup) {
};

TEST(CLIRegexTestGroup, domain) {
    boost::cmatch matches;

    CHECK(boost::regex_match("server", matches, regex::domain));
    CHECK(boost::regex_match("server:9876", matches, regex::domain));
    CHECK(boost::regex_match("server.com", matches, regex::domain));
    CHECK(boost::regex_match("server.com:1234", matches, regex::domain));
    CHECK(boost::regex_match("dom0.dom1.io", matches, regex::domain));
    CHECK(boost::regex_match("dom0.dom1.io:4567", matches, regex::domain));
    CHECK(boost::regex_match("dom0-dom1.org", matches, regex::domain));
    CHECK(boost::regex_match("dom0--dom1.org", matches, regex::domain));

    CHECK_FALSE(boost::regex_match("server:port", matches, regex::domain));
    CHECK_FALSE(boost::regex_match("-server.com", matches, regex::domain));
    CHECK_FALSE(boost::regex_match("serv-.er", matches, regex::domain));
    CHECK_FALSE(boost::regex_match("serv.-er", matches, regex::domain));
    CHECK_FALSE(boost::regex_match("serv.er-", matches, regex::domain));
    CHECK_FALSE(boost::regex_match("..server.com", matches, regex::domain));
    CHECK_FALSE(boost::regex_match("serv..er.com", matches, regex::domain));
    CHECK_FALSE(boost::regex_match("serv_er.com:1234", matches, regex::domain));
}

TEST(CLIRegexTestGroup, name) {
    boost::cmatch matches;

    CHECK(boost::regex_match("image", matches, regex::name));
    CHECK(boost::regex_match("namespace/image", matches, regex::name));
    CHECK(boost::regex_match("space0/space1/image", matches, regex::name));
    CHECK(boost::regex_match("nn/nn/nn/nn/pp/dd/xx/xx/xx", matches, regex::name));
    CHECK(boost::regex_match("space-0/space1/image", matches, regex::name));
    CHECK(boost::regex_match("image_name", matches, regex::name));
    CHECK(boost::regex_match("image-name", matches, regex::name));
    CHECK(boost::regex_match("dashed--image--name", matches, regex::name));
    CHECK(boost::regex_match("image.com", matches, regex::name));

    CHECK_FALSE(boost::regex_match("", matches, regex::name));
    // invalid component initiators/terminators
    CHECK_FALSE(boost::regex_match("-image", matches, regex::name));
    CHECK_FALSE(boost::regex_match("-space/image", matches, regex::name));
    CHECK_FALSE(boost::regex_match("space/image-", matches, regex::name));
    CHECK_FALSE(boost::regex_match("space/-image", matches, regex::name));
    CHECK_FALSE(boost::regex_match("space0-/image", matches, regex::name));
    CHECK_FALSE(boost::regex_match("_image", matches, regex::name));
    // leading slash
    CHECK_FALSE(boost::regex_match("/image", matches, regex::name));
    CHECK_FALSE(boost::regex_match("/namespace/image", matches, regex::name));
    CHECK_FALSE(boost::regex_match("/space0/space1/image", matches, regex::name));
    // trailing slash
    CHECK_FALSE(boost::regex_match("image/", matches, regex::name));
    CHECK_FALSE(boost::regex_match("namespace/image/", matches, regex::name));
    CHECK_FALSE(boost::regex_match("space0/space1/image/", matches, regex::name));
    // empty namespace
    CHECK_FALSE(boost::regex_match("nam0//nam1/image", matches, regex::name));
    // invalid characters
    CHECK_FALSE(boost::regex_match("space^0/space1/image", matches, regex::name));
    CHECK_FALSE(boost::regex_match("space$0/space1/image", matches, regex::name));
    CHECK_FALSE(boost::regex_match("space0/sp@ce1/image", matches, regex::name));
    CHECK_FALSE(boost::regex_match("space0/space1/im@ge", matches, regex::name));
    // dots
    CHECK_FALSE(boost::regex_match("../image", matches, regex::name));
    CHECK_FALSE(boost::regex_match("./image", matches, regex::name));
    CHECK_FALSE(boost::regex_match("space/../image", matches, regex::name));
    CHECK_FALSE(boost::regex_match("space0/..space1/image", matches, regex::name));
    CHECK_FALSE(boost::regex_match("space0/.space1/image", matches, regex::name));
    CHECK_FALSE(boost::regex_match("spa..ce/image", matches, regex::name));

    CHECK_FALSE(boost::regex_match("_image", matches, regex::name));
}

TEST(CLIRegexTestGroup, reference) {
    boost::cmatch matches;

    // image short name
    {
        // name only
        CHECK(boost::regex_match("image", matches, regex::reference));
        CHECK(matches[1] == "image");
        CHECK_FALSE(matches[2].matched);
        CHECK_FALSE(matches[3].matched);

        // name and tag
        CHECK(boost::regex_match("image:tag", matches, regex::reference));
        CHECK(matches[1] == "image");
        CHECK(matches[2] == "tag");
        CHECK_FALSE(matches[3].matched);

        // name and tag with capitals, numbers and dashes
        CHECK(boost::regex_match("image:tAg-195", matches, regex::reference));
        CHECK(matches[1] == "image");
        CHECK(matches[2] == "tAg-195");
        CHECK_FALSE(matches[3].matched);

        // name and digest
        CHECK(boost::regex_match("image@sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83", matches, regex::reference));
        CHECK(matches[1] == "image");
        CHECK_FALSE(matches[2].matched);
        CHECK(matches[3] == "sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83");

        // name, tag and digest
        CHECK(boost::regex_match("image:tag@sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83", matches, regex::reference));
        CHECK(matches[1] == "image");
        CHECK(matches[2] == "tag");
        CHECK(matches[3] == "sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83");
    }
    // namespace and image
    {
        // name only
        CHECK(boost::regex_match("namespace/image", matches, regex::reference));
        CHECK(matches[1] == "namespace/image");
        CHECK_FALSE(matches[2].matched);
        CHECK_FALSE(matches[3].matched);

        // name and tag
        CHECK(boost::regex_match("namespace/image:tag", matches, regex::reference));
        CHECK(matches[1] == "namespace/image");
        CHECK(matches[2] == "tag");
        CHECK_FALSE(matches[3].matched);

        // name and digest
        CHECK(boost::regex_match("namespace/image@sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83", matches, regex::reference));
        CHECK(matches[1] == "namespace/image");
        CHECK_FALSE(matches[2].matched);
        CHECK(matches[3] == "sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83");

        // name, digest and tag
        CHECK(boost::regex_match("namespace/image:tag@sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83", matches, regex::reference));
        CHECK(matches[1] == "namespace/image");
        CHECK(matches[2] == "tag");
        CHECK(matches[3] == "sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83");
    }
    // domain, namespace and image
    {
        // name only
        CHECK(boost::regex_match("server.io/namespace/image", matches, regex::reference));
        CHECK(matches[1] == "server.io/namespace/image");
        CHECK_FALSE(matches[2].matched);
        CHECK_FALSE(matches[3].matched);

        // name only with port on domain
        CHECK(boost::regex_match("server.io:1234/namespace/image", matches, regex::reference));
        CHECK(matches[1] == "server.io:1234/namespace/image");
        CHECK_FALSE(matches[2].matched);
        CHECK_FALSE(matches[3].matched);

        // name and tag
        CHECK(boost::regex_match("server.io:1234/namespace/image:tag", matches, regex::reference));
        CHECK(matches[1] == "server.io:1234/namespace/image");
        CHECK(matches[2] == "tag");
        CHECK_FALSE(matches[3].matched);

        // name and digest
        CHECK(boost::regex_match("server.io:1234/namespace/image@sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83", matches, regex::reference));
        CHECK(matches[1] == "server.io:1234/namespace/image");
        CHECK_FALSE(matches[2].matched);
        CHECK(matches[3] == "sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83");

        // name, image and tag
        CHECK(boost::regex_match("server.io:1234/namespace/image:tag@sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83", matches, regex::reference));
        CHECK(matches[1] == "server.io:1234/namespace/image");
        CHECK(matches[2] == "tag");
        CHECK(matches[3] == "sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83");

        // missing namespace
        CHECK(boost::regex_match("server.io:1234/image:tag@sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83", matches, regex::reference));
        CHECK(matches[1] == "server.io:1234/image");
        CHECK(matches[2] == "tag");
        CHECK(matches[3] == "sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83");

        // missing namespace and name, localhost is treated as name, port is treated as tag
        CHECK(boost::regex_match("localhost:1234@sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83", matches, regex::reference));
        CHECK(matches[1] == "localhost");
        CHECK(matches[2] == "1234");
        CHECK(matches[3] == "sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83");

        // multiple nested namespaces in name
        CHECK(boost::regex_match("server.io:1234/namespace0/namespace1/namespace2/image:tag@sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83", matches, regex::reference));
        CHECK(matches[1] == "server.io:1234/namespace0/namespace1/namespace2/image");
        CHECK(matches[2] == "tag");
        CHECK(matches[3] == "sha256:d4ff818577bc193b309b355b02ebc9220427090057b54a59e73b79bdfe139b83");
    }
    // invalid strings
    {
        CHECK_FALSE(boost::regex_match("server.io:1234/namespace/image:invalid~tag", matches, regex::reference));
        CHECK_FALSE(boost::regex_match("server.io:1234/namespace/image@hashlessdigest", matches, regex::reference));
    }
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
