/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "image_manager/Puller.hpp"
#include "image_manager/PulledImage.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace image_manager {
namespace test {

TEST_GROUP(PulledImageTestGroup) {
};

TEST(PulledImageTestGroup, test) {
    auto configRAII = test_utility::config::makeConfig();
    configRAII.config->imageID = {"quay.io", "ethcscs", "alpine", "3.14"};

    // pull
    auto puller = image_manager::Puller{configRAII.config};
    auto manifest = puller.retrieveImageManifest();
    puller.pull();

    // expand
    auto pulledImage = PulledImage{configRAII.config, manifest};
    common::PathRAII expandedImage;
    common::ImageMetadata metadata;
    std::tie(expandedImage, metadata, std::ignore) = pulledImage.expand();

    // check
    auto expectedDirectory = expandedImage.getPath() / "etc";
    CHECK(boost::filesystem::exists(expectedDirectory));

    auto expectedFile = expandedImage.getPath() / "etc/os-release";
    CHECK(boost::filesystem::exists(expectedFile));

    auto expectedMetadata = common::ImageMetadata{};
    expectedMetadata.cmd = common::CLIArguments{"/bin/sh"};
    expectedMetadata.env["PATH"] = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    CHECK(metadata == expectedMetadata);
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
