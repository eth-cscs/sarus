/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <memory>

#include "image_manager/LoadedImage.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace image_manager {
namespace test {

TEST_GROUP(LoadedImageTestGroup) {
};

TEST(LoadedImageTestGroup, test) {
    auto configRAII = test_utility::config::makeConfig();

    // expand
    auto archive = boost::filesystem::path{__FILE__}.parent_path() / "saved_image.tar";
    auto loadedImage = LoadedImage{configRAII.config, archive};
    common::PathRAII expandedImage;
    common::ImageMetadata metadata;
    std::tie(expandedImage, metadata, std::ignore) = loadedImage.expand();

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

TEST(LoadedImageTestGroup, image_with_nonexecutable_directory) {
    auto configRAII = test_utility::config::makeConfig();
    auto archive = boost::filesystem::path{__FILE__}.parent_path() / "saved_image_with_non-executable_dir.tar";
    auto loadedImage = LoadedImage{configRAII.config, archive};
    loadedImage.expand();
}

TEST(LoadedImageTestGroup, image_with_malicious_files) {
    auto configRAII = test_utility::config::makeConfig();
    auto archive = boost::filesystem::path{__FILE__}.parent_path() / "saved_image_malicious.tar";
    auto loadedImage = LoadedImage{configRAII.config, archive};
    loadedImage.expand();

    // Check:
    // the image was manually forged in order to create "malicious" files in /tmp,
    // i.e. files meant to escape the expansion root directory. These checks make
    // sure that those files are correctly "blocked" and don't exist in /tmp.
    CHECK(!boost::filesystem::exists("/tmp/malicious-file-through-abs-path"));
    CHECK(!boost::filesystem::exists("/tmp/malicious-file-through-dotdots"));
    CHECK(!boost::filesystem::exists("/tmp/malicious-file-through-symlink"));
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
