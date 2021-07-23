/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_common_ImageID_hpp
#define sarus_common_ImageID_hpp

#include <string>
#include <ostream>


namespace sarus {
namespace common {

struct ImageID {
    std::string server;
    std::string repositoryNamespace;
    std::string image;
    std::string tag;
    std::string getUniqueKey() const;

    static const std::string DEFAULT_SERVER;
    static const std::string DEFAULT_REPOSITORY_NAMESPACE;
};

bool operator==(const ImageID&, const ImageID&);

std::ostream& operator<<(std::ostream&, const ImageID&);

}
}

#endif
