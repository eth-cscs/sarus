/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/ImageReference.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace common {
namespace test {

TEST_GROUP(ImageReferenceTestGroup) {
};

TEST(ImageReferenceTestGroup, getFullName) {
    // all members
    auto ref = common::ImageReference{"server", "namespace", "image", "tag", "sha256:1234567890abcdef"};
    CHECK_EQUAL(ref.getFullName(), std::string{"server/namespace/image"});

    // no digest
    ref = common::ImageReference{"server", "namespace", "image", "tag", ""};
    CHECK_EQUAL(ref.getFullName(), std::string{"server/namespace/image"});

    // no tag
    ref = common::ImageReference{"server", "namespace", "image", "", "sha256:1234567890abcdef"};
    CHECK_EQUAL(ref.getFullName(), std::string{"server/namespace/image"});

    // no tag and no digest
    ref = common::ImageReference{"server", "namespace", "image", "", ""};
    CHECK_EQUAL(ref.getFullName(), std::string{"server/namespace/image"});
}

TEST(ImageReferenceTestGroup, string) {
    // default values
    auto ref = common::ImageReference{common::ImageReference::DEFAULT_SERVER,
                                 common::ImageReference::DEFAULT_REPOSITORY_NAMESPACE,
                                 "image",
                                 common::ImageReference::DEFAULT_TAG,
                                 ""};
    CHECK_EQUAL(ref.string(), std::string{"index.docker.io/library/image:latest"});

    // all members
    ref = common::ImageReference{"server", "namespace", "image", "tag", "sha256:1234567890abcdef"};
    CHECK_EQUAL(ref.string(), std::string{"server/namespace/image:tag@sha256:1234567890abcdef"});

    // no digest
    ref = common::ImageReference{"server", "namespace", "image", "tag", ""};
    CHECK_EQUAL(ref.string(), std::string{"server/namespace/image:tag"});

    // no tag
    ref = common::ImageReference{"server", "namespace", "image", "", "sha256:1234567890abcdef"};
    CHECK_EQUAL(ref.string(), std::string{"server/namespace/image@sha256:1234567890abcdef"});

    // no tag and no digest
    ref = common::ImageReference{"server", "namespace", "image", "", ""};
    CHECK_EQUAL(ref.string(), std::string{"server/namespace/image"});
}

TEST(ImageReferenceTestGroup, normalize) {
    // all members
    auto ref = common::ImageReference{"server", "namespace", "image", "tag", "sha256:1234567890abcdef"};
    CHECK_EQUAL(ref.normalize(), std::string{"server/namespace/image@sha256:1234567890abcdef"});

    // no digest
    ref = common::ImageReference{"server", "namespace", "image", "tag", ""};
    CHECK_EQUAL(ref.normalize(), std::string{"server/namespace/image:tag"});

    // no tag
    ref = common::ImageReference{"server", "namespace", "image", "", "sha256:1234567890abcdef"};
    CHECK_EQUAL(ref.normalize(), std::string{"server/namespace/image@sha256:1234567890abcdef"});

    // no tag and no digest
    ref = common::ImageReference{"server", "namespace", "image", "", ""};
    CHECK_EQUAL(ref.normalize(), std::string{"server/namespace/image"});

}

TEST(ImageReferenceTestGroup, getUniqueKey) {
    // all members
    auto ref = common::ImageReference{"server", "namespace", "image", "tag", "sha256:1234567890abcdef"};
    CHECK_EQUAL(ref.getUniqueKey(), std::string{"server/namespace/image/tag"});

    // no digest
    ref = common::ImageReference{"server", "namespace", "image", "tag", ""};
    CHECK_EQUAL(ref.getUniqueKey(), std::string{"server/namespace/image/tag"});

    // no tag
    ref = common::ImageReference{"server", "namespace", "image", "", "sha256:1234567890abcdef"};
    CHECK_EQUAL(ref.getUniqueKey(), std::string{"server/namespace/image/sha256-1234567890abcdef"});

    // no tag and no digest
    ref = common::ImageReference{"server", "namespace", "image", "", ""};
    CHECK_THROWS(common::Error, ref.getUniqueKey());

    // multiple namespaces
    ref = common::ImageReference{"server", "namespace0/namespace1", "image", "tag", ""};
    CHECK_EQUAL(ref.getUniqueKey(), std::string{"server/namespace0/namespace1/image/tag"});

}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
