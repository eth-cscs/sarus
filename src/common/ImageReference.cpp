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

#include <boost/format.hpp>


namespace sarus {
namespace common {

const std::string ImageReference::DEFAULT_SERVER{"index.docker.io"};
const std::string ImageReference::DEFAULT_REPOSITORY_NAMESPACE{"library"};

std::string ImageReference::string() const {
    auto format =  boost::format{"%s/%s/%s:%s"}
            % server
            % repositoryNamespace
            % image
            % tag;
    return format.str();
}

bool operator==(const ImageReference& lhs, const ImageReference& rhs) {
    return lhs.server == rhs.server
        && lhs.repositoryNamespace == rhs.repositoryNamespace
        && lhs.image == rhs.image
        && lhs.tag == rhs.tag;
}

std::ostream& operator<<(std::ostream& os, const ImageReference& imageReference) {
    os << imageReference.string();
    return os;
}

std::string ImageReference::getUniqueKey() const {
    auto format =  boost::format{"%s/%s/%s/%s"};
    return (format
            % server
            % repositoryNamespace
            % image
            % tag).str();
}

}
}
