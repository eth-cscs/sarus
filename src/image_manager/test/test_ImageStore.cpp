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

#include <boost/filesystem.hpp>

#include "test_utility/config.hpp"
#include "common/Utility.hpp"
#include "image_manager/ImageStore.hpp" 
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(ImageStoreTestGroup) {
};

TEST(ImageStoreTestGroup, test_ImageStore) {
    auto configRAII = test_utility::config::makeConfig();
    configRAII.config->imageID = {"index.docker.io", "library", "hello-world", "latest"};

    std::string dummyDigest = "XXXdigestXXX";
    time_t currentTime = time_t(nullptr);

    auto image = common::SarusImage{
        configRAII.config->imageID,
        dummyDigest,
        common::SarusImage::createSizeString(size_t(1024)),
        common::SarusImage::createTimeString(currentTime),
        configRAII.config->getImageFile(),
        configRAII.config->getMetadataFileOfImage()};

    auto imageStore = image_manager::ImageStore{ configRAII.config };

    // clean repository
    boost::filesystem::remove_all(configRAII.config->directories.repository);
    common::createFoldersIfNecessary(configRAII.config->directories.repository);
    CHECK(imageStore.listImages().empty());

    // add image
    imageStore.addImage(image);
    CHECK(boost::filesystem::exists(imageStore.getMetadataFile()));
    auto expectedImages = std::vector<common::SarusImage>{ image };
    CHECK(imageStore.listImages() == expectedImages);

    // add same image another time
    imageStore.addImage(image);
    CHECK(boost::filesystem::exists(imageStore.getMetadataFile()));
    CHECK(imageStore.listImages() == expectedImages);

    // remove image
    imageStore.removeImage(configRAII.config->imageID);
    CHECK(imageStore.listImages().empty());
}

SARUS_UNITTEST_MAIN_FUNCTION();
