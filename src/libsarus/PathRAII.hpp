/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_PathRAII_hpp
#define libsarus_PathRAII_hpp

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

namespace libsarus {

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
    void setFilesAsRemovableByOwner() const;

    boost::optional<boost::filesystem::path> path;
};

}

#endif
