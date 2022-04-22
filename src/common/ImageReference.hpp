/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_common_ImageReference_hpp
#define sarus_common_ImageReference_hpp

#include <string>
#include <ostream>


namespace sarus {
namespace common {

struct ImageReference {
    std::string server;
    std::string repositoryNamespace;
    std::string image;
    std::string tag;
    std::string digest;
    std::string getFullName() const;
    std::string string() const;
    std::string getUniqueKey() const;
    ImageReference normalize() const;

    static const std::string DEFAULT_SERVER;
    static const std::string DEFAULT_REPOSITORY_NAMESPACE;
    static const std::string DEFAULT_TAG;
};

bool operator==(const ImageReference&, const ImageReference&);

std::ostream& operator<<(std::ostream&, const ImageReference&);

}
}

#endif
