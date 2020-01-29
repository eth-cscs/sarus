/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_common_PathHash_hpp
#define sarus_common_PathHash_hpp

#include <boost/filesystem.hpp>

namespace sarus {
namespace common {

class PathHash {
public:
    size_t operator()(const boost::filesystem::path& path) const {
        return boost::filesystem::hash_value(path);
    }
};

}} // namespace

#endif
