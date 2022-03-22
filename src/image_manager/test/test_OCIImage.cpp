/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "image_manager/Puller.hpp"
#include "image_manager/OCIImage.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace image_manager {
namespace test {

TEST_GROUP(OCIImageTestGroup) {
};

TEST(OCIImageTestGroup, testExpand) {
    auto configRAII = test_utility::config::makeConfig();
    auto imagePath = boost::filesystem::path{__FILE__}.parent_path() / "saved_image_oci";
    auto ociImage = OCIImage{configRAII.config, imagePath};

    auto expandedImage = ociImage.expand();

    auto expectedDirectory = expandedImage.getPath() / "etc";
    CHECK(boost::filesystem::exists(expectedDirectory));

    auto expectedFile = expandedImage.getPath() / "etc/os-release";
    CHECK(boost::filesystem::exists(expectedFile));

    // Release the internal PathRAII so the OCIImage dtor does not remove the "saved_image_oci"
    // test artifact
    ociImage.release();
}

TEST(OCIImageTestGroup, testMetadata) {
    auto configRAII = test_utility::config::makeConfig();
    auto imagePath = boost::filesystem::path{__FILE__}.parent_path() / "saved_image_oci";
    auto ociImage = OCIImage{configRAII.config, imagePath};

    auto expectedMetadata = common::ImageMetadata{};
    expectedMetadata.cmd = common::CLIArguments{"/bin/sh"};
    expectedMetadata.env["PATH"] = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    CHECK(ociImage.getMetadata() == expectedMetadata);

    // Release the internal PathRAII so the OCIImage dtor does not remove the "saved_image_oci"
    // test artifact
    ociImage.release();
}

TEST(OCIImageTestGroup, testImageID) {
    auto configRAII = test_utility::config::makeConfig();
    auto imagePath = boost::filesystem::path{__FILE__}.parent_path() / "saved_image_oci";
    auto ociImage = OCIImage{configRAII.config, imagePath};

    auto expectedImageID = std::string{"501d1a8f0487e93128df34ea349795bc324d5e0c0d5112e08386a9dfaff620be"};
    CHECK(ociImage.getImageID() == expectedImageID);

    // Release the internal PathRAII so the OCIImage dtor does not remove the "saved_image_oci"
    // test artifact
    ociImage.release();
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
