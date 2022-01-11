/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_common_PathRAII_hpp
#define sarus_common_PathRAII_hpp

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>


namespace sarus {
namespace common {

// RAII wrapper for a path: manages the lifetime of a specified path,
// which is automatically removed by the destructor of this class.
class PathRAII {
public:
    PathRAII() = default;
    PathRAII(const boost::filesystem::path& path);
    PathRAII(const PathRAII&) = delete;
    PathRAII(PathRAII&&);
    PathRAII& operator=(const PathRAII&) = delete;
    PathRAII& operator=(PathRAII&&);
    ~PathRAII();

    const boost::filesystem::path& getPath() const;
    void release();

private:
    boost::optional<boost::filesystem::path> path;
};

}
}

#endif
