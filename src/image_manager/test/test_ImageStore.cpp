/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
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

namespace sarus {
namespace image_manager {
namespace test {

TEST_GROUP(ImageStoreTestGroup) {
    test_utility::config::ConfigRAII configRAII = test_utility::config::makeConfig();;
    image_manager::ImageStore imageStore = image_manager::ImageStore{ configRAII.config };
    std::vector<common::ImageReference> refVector{};
    std::vector<common::SarusImage> imageVector{};

    void setup()
    {
        auto helloWorldRef    = common::ImageReference{"index.docker.io", "library", "hello-world", "latest", "sha256:hello-world-digest"};
        auto alpineTaglessRef = common::ImageReference{"index.docker.io", "library", "alpine", "", "sha256:alpine-tagless-digest"};
        auto alpineLatestRef  = common::ImageReference{"index.docker.io", "library", "alpine", "latest", "sha256:alpine-latest-digest"};
        auto quayUbuntuRef    = common::ImageReference{"quay.io", "ethcscs", "ubuntu", "20.04", "sha256:quayio-ubuntu-digest"};
        std::string dummyID = "1234567890abcdef";
        time_t currentTime = time_t(nullptr);

        refVector = std::vector<common::ImageReference>{helloWorldRef, alpineTaglessRef, alpineLatestRef, quayUbuntuRef};
        for (const auto& ref : refVector) {
            imageVector.push_back(common::SarusImage{
                                      ref,
                                      dummyID,
                                      common::SarusImage::createSizeString(size_t(1024)),
                                      common::SarusImage::createTimeString(currentTime),
                                      imageStore.getImageSquashfsFile(ref),
                                      imageStore.getImageMetadataFile(ref)
                                  });
        }
    }
};

void addImageHarness(const image_manager::ImageStore& imageStore, const common::SarusImage& image) {
    imageStore.addImage(image);
    common::createFileIfNecessary(image.imageFile);
    common::createFileIfNecessary(image.metadataFile);
}

TEST(ImageStoreTestGroup, addListRemove) {
    // add images and check repo metadata has been correctly populated
    for (const auto& image : imageVector) {
        addImageHarness(imageStore, image);
    }
    CHECK(boost::filesystem::exists(imageStore.getRepositoryMetadataFile()));
    CHECK(imageStore.listImages() == imageVector);

    // automatically remove an image without backing file
    boost::filesystem::remove(imageVector.back().imageFile);
    imageVector.pop_back();
    refVector.pop_back();
    CHECK(imageStore.listImages() == imageVector);

    // remove all remaining images
    for (const auto& ref : refVector) {
        imageStore.removeImage(ref);
    }
    CHECK(imageStore.listImages().empty());

    // re-fill the repository and add an existing image another time
    // Note that addImage() does not care whether an image exists or not, it will always go through
    // removing a previously existing entry and pushing a new element to the back of the repository metadata.
    // Thus, if we re-add the first image, the repository metadata array will "rotate" of 1 position
    for (const auto& image : imageVector) {
        addImageHarness(imageStore, image);
    }
    CHECK(imageStore.listImages() == imageVector);
    addImageHarness(imageStore, imageVector[0]);
    CHECK(imageStore.listImages().back() == imageVector[0]);
    CHECK(imageStore.listImages().front() == imageVector[1]);
}

TEST(ImageStoreTestGroup, getImageID) {
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
    // add images and create dummy backing files
    for (const auto& image : imageVector) {
        addImageHarness(imageStore, image);
    }
    CHECK(boost::filesystem::exists(imageStore.getRepositoryMetadataFile()));
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

    // automatically remove an image without backing file
    boost::filesystem::remove(imageVector.back().imageFile);
    CHECK_FALSE(imageStore.findImage(refVector.back()));
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
