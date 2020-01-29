/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string>

#include "common/Utility.hpp"
#include "common/Config.hpp"
#include "cli/Utility.hpp"
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(CLIUtilityTestGroup) {
};

TEST(CLIUtilityTestGroup, isValidCLIInputImageID) {
    // valid image ids
    CHECK_TRUE(cli::utility::isValidCLIInputImageID("image"));
    CHECK_TRUE(cli::utility::isValidCLIInputImageID("image:tag"));
    CHECK_TRUE(cli::utility::isValidCLIInputImageID("namespace/image:tag"));
    CHECK_TRUE(cli::utility::isValidCLIInputImageID("server/namespace/image:tag"));
    
    // invalid image ids
    CHECK_FALSE(cli::utility::isValidCLIInputImageID("../image"));

    CHECK_FALSE(cli::utility::isValidCLIInputImageID("../image:tag"));
    CHECK_FALSE(cli::utility::isValidCLIInputImageID("image/..:tag"));
    CHECK_FALSE(cli::utility::isValidCLIInputImageID("image:../tag"));

    CHECK_FALSE(cli::utility::isValidCLIInputImageID("../namespace/image:tag"));
    CHECK_FALSE(cli::utility::isValidCLIInputImageID("namespace/../image:tag"));

    CHECK_FALSE(cli::utility::isValidCLIInputImageID("../server/namespace/image:tag"));
    CHECK_FALSE(cli::utility::isValidCLIInputImageID("server/../image:tag"));
}

TEST(CLIUtilityTestGroup, parseImageID) {
    auto imageID = common::ImageID{};

    imageID = cli::utility::parseImageID("image");
    CHECK_EQUAL(imageID.server, std::string{"index.docker.io"});
    CHECK_EQUAL(imageID.repositoryNamespace, std::string{"library"});
    CHECK_EQUAL(imageID.image, std::string{"image"});
    CHECK_EQUAL(imageID.tag, std::string{"latest"});

    imageID = cli::utility::parseImageID("image:tag");
    CHECK_EQUAL(imageID.server, std::string{"index.docker.io"});
    CHECK_EQUAL(imageID.repositoryNamespace, std::string{"library"});
    CHECK_EQUAL(imageID.image, std::string{"image"});
    CHECK_EQUAL(imageID.tag, std::string{"tag"});

    imageID = cli::utility::parseImageID("namespace/image:tag");
    CHECK_EQUAL(imageID.server, std::string{"index.docker.io"});
    CHECK_EQUAL(imageID.repositoryNamespace, std::string{"namespace"});
    CHECK_EQUAL(imageID.image, std::string{"image"});
    CHECK_EQUAL(imageID.tag, std::string{"tag"});

    imageID = cli::utility::parseImageID("server/namespace/image:tag");
    CHECK_EQUAL(imageID.server, std::string{"server"});
    CHECK_EQUAL(imageID.repositoryNamespace, std::string{"namespace"});
    CHECK_EQUAL(imageID.image, std::string{"image"});
    CHECK_EQUAL(imageID.tag, std::string{"tag"});
}

SARUS_UNITTEST_MAIN_FUNCTION();
