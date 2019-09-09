/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "ImageID.hpp"

#include <boost/format.hpp>


namespace sarus {
namespace common {

const std::string ImageID::DEFAULT_SERVER{"index.docker.io"};
const std::string ImageID::DEFAULT_REPOSITORY_NAMESPACE{"library"};

bool operator==(const ImageID& lhs, const ImageID& rhs) {
    return lhs.server == rhs.server
        && lhs.repositoryNamespace == rhs.repositoryNamespace
        && lhs.image == rhs.image
        && lhs.tag == rhs.tag;
}

std::ostream& operator<<(std::ostream& os, const ImageID& imageID) {
    auto format =  boost::format{"%s/%s/%s:%s"}
        % imageID.server
        % imageID.repositoryNamespace
        % imageID.image
        % imageID.tag;
    os << format;
    return os;
}

std::string ImageID::getUniqueKey() const {
    auto format =  boost::format{"%s/%s/%s/%s"};
    return (format
            % server
            % repositoryNamespace
            % image
            % tag).str();
}

}
}
