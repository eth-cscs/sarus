/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "ImageReference.hpp"

#include <sstream>
#include <algorithm>

#include <boost/format.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "common/Error.hpp"


namespace sarus {
namespace common {

const std::string ImageReference::DEFAULT_SERVER{"index.docker.io"};
const std::string ImageReference::DEFAULT_REPOSITORY_NAMESPACE{"library"};
const std::string ImageReference::DEFAULT_TAG{"latest"};

std::string ImageReference::getFullName() const {
    auto format =  boost::format{"%s/%s/%s"}
            % server
            % repositoryNamespace
            % image;
    return format.str();
}

std::string ImageReference::string() const {
    auto output = std::stringstream{};
    output << getFullName();
    if (!tag.empty()){
        output << ":" << tag;
    }
    if (!digest.empty()){
        output << "@" << digest;
    }
    return output.str();
}

/**
 * Normalizing a reference means clearing the tag if the digest is also present.
 * This is useful to reproduce Docker's behavior, which completely ignores the tag
 * when a digest is given. Podman and Buildah also implement this behavior for
 * compatibility.
 * See for reference: https://github.com/containers/common/pull/579,
 * https://github.com/containers/common/blob/v0.47.4/libimage/normalize.go
 */
ImageReference ImageReference::normalize() const {
    auto output = *this;
    if (!digest.empty() && !tag.empty()){
        output.tag.clear();
    }
    return output;
}

bool operator==(const ImageReference& lhs, const ImageReference& rhs) {
    return lhs.server == rhs.server
        && lhs.repositoryNamespace == rhs.repositoryNamespace
        && lhs.image == rhs.image
        && lhs.tag == rhs.tag
        && lhs.digest == rhs.digest;
}

std::ostream& operator<<(std::ostream& os, const ImageReference& imageReference) {
    os << imageReference.string();
    return os;
}

/**
 * Creates a string which can be used to construct a filesystem path to the location
 * of squashfs and metadata files for an image within a Sarus local repository.
 * The tag has higher priority than the digest because:
 *
 * - files for image pulled by tag are separate than files for images pulled by digest
 * - ImageManager::pullImage() completes the reference with a digest in case a digest is
 *   not supplied vie the CLI. This is consistent with the behavior of Docker.
 *
 * Therefore, if getUniqueKey() gave higher priority to the digest, when a tag+digest
 * reference is used in the CLI, the consumers of the uniqueKey would fail to find the
 * files named after the tag, or worse delete the files named after the digest.
 *
 * This situation could be streamlined in the future by only storing images by digest and
 * having ImageStore resolve tags to the correct images/files stored.
 */
std::string ImageReference::getUniqueKey() const {
    auto output = std::stringstream{};
    output << getFullName();
    if (!tag.empty()){
        output << "/" << tag;
    }
    else if (!digest.empty()){
        // Replace the colon in the digest with a dash, which is less problematic when used in paths
        auto outDigest = digest;
        std::replace_if(outDigest.begin(), outDigest.end(), boost::is_any_of(":"), '-');
        output << "/" << outDigest;
    }
    else {
        auto message = boost::format("Malformed ImageReference: %s\n"
                                     "Must have either a tag, a digest or both to create a unique key") % string();
        SARUS_THROW_ERROR(message.str());
    }
    return output.str();
}

}
}
