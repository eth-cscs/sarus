/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "image_manager/OCIImage.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace image_manager {
namespace test {

TEST_GROUP(OCIImageTestGroup) {
};

TEST(OCIImageTestGroup, unpack) {
    auto configRAII = test_utility::config::makeConfig();
    auto imagePath = boost::filesystem::path{__FILE__}.parent_path() / "saved_image_oci";
    auto ociImage = OCIImage{configRAII.config, imagePath};

    auto unpackedImage = ociImage.unpack();

    auto expectedDirectory = unpackedImage.getPath() / "etc";
    CHECK(boost::filesystem::exists(expectedDirectory));

    auto expectedFile = unpackedImage.getPath() / "etc/os-release";
    CHECK(boost::filesystem::exists(expectedFile));

    // Release the internal PathRAII so the OCIImage dtor does not remove the "saved_image_oci"
    // test artifact
    ociImage.release();
}

TEST(OCIImageTestGroup, getMetadata) {
    auto configRAII = test_utility::config::makeConfig();
    auto imagePath = boost::filesystem::path{__FILE__}.parent_path() / "saved_image_oci";
    auto ociImage = OCIImage{configRAII.config, imagePath};

    auto expectedMetadata = common::ImageMetadata{};
    expectedMetadata.cmd = common::CLIArguments{"/bin/sh"};
    expectedMetadata.env["PATH"] = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    expectedMetadata.labels["com.example.project.git.url"] = "https://example.com/project.git";
    expectedMetadata.labels["com.example.project.git.commit"] = "45a939b2999782a3f005621a8d0f29aa387e1d6b";
    expectedMetadata.labels["com.test.engine.name"] = "sarus";
    CHECK(ociImage.getMetadata() == expectedMetadata);

    // Release the internal PathRAII so the OCIImage dtor does not remove the "saved_image_oci"
    // test artifact
    ociImage.release();
}

TEST(OCIImageTestGroup, getImageID) {
    auto configRAII = test_utility::config::makeConfig();
    auto imagePath = boost::filesystem::path{__FILE__}.parent_path() / "saved_image_oci";
    auto ociImage = OCIImage{configRAII.config, imagePath};

    auto expectedImageID = std::string{"2c2372178e530e6207e05f0756bb4b3018a92f62616c4af5fd4c42eb361e6079"};
    CHECK(ociImage.getImageID() == expectedImageID);

    // Release the internal PathRAII so the OCIImage dtor does not remove the "saved_image_oci"
    // test artifact
    ociImage.release();
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
