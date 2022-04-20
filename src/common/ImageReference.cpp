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

std::string ImageReference::normalize() const {
    auto output = std::stringstream{};
    output << getFullName();
    if (!digest.empty()){
        output << "@" << digest;
    }
    else if (!tag.empty()){
        output << ":" << tag;
    }
    return output.str();
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
