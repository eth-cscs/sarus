/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_PathHash_hpp
#define libsarus_PathHash_hpp

#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>

namespace libsarus {

class PathHash {
public:
    size_t operator()(const boost::filesystem::path& path) const {
        return boost::filesystem::hash_value(path);
    }
};

}

#endif
