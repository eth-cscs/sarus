/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <memory>

#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "test_utility/config.hpp"
#include "common/Utility.hpp"
#include "image_manager/ImageStore.hpp" 
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(ImageStoreTestGroup) {
};

TEST(ImageStoreTestGroup, test_ImageStore) {
    auto configRAII = test_utility::config::makeConfig();
    configRAII.config->imageReference = {"index.docker.io", "library", "hello-world", "latest", "sha256:1234567890abcdef"};

    auto imageStore = image_manager::ImageStore{ configRAII.config };

    std::string dummyID = "a9561eb1b190625c9adb5a9513e72c4dedafc1cb2d4c5236c9a6957ec7dfd5a9";
    time_t currentTime = time_t(nullptr);

    auto image = common::SarusImage{
        configRAII.config->imageReference,
        dummyID,
        common::SarusImage::createSizeString(size_t(1024)),
        common::SarusImage::createTimeString(currentTime),
        imageStore.getImageFile(configRAII.config->imageReference),
        imageStore.getMetadataFileOfImage(configRAII.config->imageReference)};

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
    imageStore.removeImage(configRAII.config->imageReference);
    CHECK(imageStore.listImages().empty());
}

TEST(ImageStoreTestGroup, getImageID) {
    auto configRAII = test_utility::config::makeConfig();
    auto imageStore = image_manager::ImageStore{ configRAII.config };
    auto document = rapidjson::Document{};
    auto allocator = document.GetAllocator();

    // id property is present
    auto metadata = rapidjson::Value{rapidjson::kObjectType};
    metadata.AddMember("image", rapidjson::Value{"test-image", allocator},
                       allocator);
    metadata.AddMember("tag", rapidjson::Value{"latest", allocator},
                       allocator);
    metadata.AddMember("id", rapidjson::Value{"test-image-id-01234", allocator},
                       allocator);
    CHECK(imageStore.getImageID(metadata) == std::string{"test-image-id-01234"});

    // id property is not present
    metadata.RemoveMember("id");
    CHECK(imageStore.getImageID(metadata).empty());
}

TEST(ImageStoreTestGroup, findImage) {
    auto configRAII = test_utility::config::makeConfig();
    auto imageStore = image_manager::ImageStore{ configRAII.config };

    // clean repository
    boost::filesystem::remove_all(configRAII.config->directories.repository);
    common::createFoldersIfNecessary(configRAII.config->directories.repository);
    CHECK(imageStore.listImages().empty());

    // generate image objects
    auto helloWorldRef    = common::ImageReference{"index.docker.io", "library", "hello-world", "latest", "sha256:hello-world-digest"};
    auto alpineTaglessRef = common::ImageReference{"index.docker.io", "library", "alpine", "", "sha256:alpine-tagless-digest"};
    auto alpineLatestRef  = common::ImageReference{"index.docker.io", "library", "alpine", "latest", "sha256:alpine-latest-digest"};
    auto quayUbuntuRef    = common::ImageReference{"quay.io", "ethcscs", "ubuntu", "20.04", "sha256:quayio-ubuntu-digest"};

    std::string dummyID = "1234567890abcdef";
    time_t currentTime = time_t(nullptr);

    auto refVector = std::vector<common::ImageReference>{helloWorldRef, alpineTaglessRef, alpineLatestRef, quayUbuntuRef};
    auto imageVector = std::vector<common::SarusImage>{};

    for (const auto& ref : refVector) {
        imageVector.push_back(common::SarusImage{
                                  ref,
                                  dummyID,
                                  common::SarusImage::createSizeString(size_t(1024)),
                                  common::SarusImage::createTimeString(currentTime),
                                  imageStore.getImageFile(ref),
                                  imageStore.getMetadataFileOfImage(ref)
                              });
    }

    // add images
    for (const auto& image : imageVector) {
        imageStore.addImage(image);
    }
    CHECK(boost::filesystem::exists(imageStore.getMetadataFile()));
    CHECK(imageStore.listImages().size() == imageVector.size());

    // look for available images
    CHECK(imageStore.findImage(refVector[0]).value() == imageVector[0]);
    CHECK(imageStore.findImage(refVector[1]).value() == imageVector[1]);
    CHECK(imageStore.findImage(refVector[2]).value() == imageVector[2]);
    CHECK(imageStore.findImage(refVector[3]).value() == imageVector[3]);

    // look for non-available images
    CHECK_FALSE(imageStore.findImage({"index.docker.io", "library", "fedora", "35", "sha256:fedora-35-digest"}));
    CHECK_FALSE(imageStore.findImage({"index.docker.io", "library", "alpine", "", "sha256:non-available-digest"}));

    // look an available tagged image by digest
    CHECK_FALSE(imageStore.findImage({"index.docker.io", "library", "alpine", "", "sha256:alpine-latest-digest"}));
}

SARUS_UNITTEST_MAIN_FUNCTION();
