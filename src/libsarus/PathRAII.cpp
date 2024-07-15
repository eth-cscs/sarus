/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "PathRAII.hpp"
#include "Error.hpp"
#include "Utility.hpp"

namespace libsarus {

PathRAII::PathRAII(const boost::filesystem::path& path)
    : path{path}
{
    try {
        this->path = path;
    }
    catch(const std::exception& e) {
        boost::filesystem::remove_all(path);
        SARUS_RETHROW_ERROR(e, "Failed to initialize PathRAII's path member");
    }
}

PathRAII::PathRAII(PathRAII&& rhs)
    : path{std::move(rhs.path)}
{
    rhs.release();
}

PathRAII& PathRAII::operator=(PathRAII&& rhs) {
    path = std::move(rhs.path);
    rhs.release();
    return *this;
}

PathRAII::~PathRAII() {
    if(path && boost::filesystem::exists(*path)) {
        // Ensure the path contents can be removed by enforcing owner write and exec permissions.
        // This is needed when using PathRAII for unpacked OCI image files, because
        // some images (e.g. Fedora) contain files without owner write or search perms
        setFilesAsRemovableByOwner();
        boost::filesystem::remove_all(*path);
    }
}

const boost::filesystem::path& PathRAII::getPath() const {
    return path.value();
}

void PathRAII::release() {
    path.reset();
}

void PathRAII::setFilesAsRemovableByOwner() const {
    auto requiredPermissions = boost::filesystem::perms::owner_write | boost::filesystem::perms::owner_exe;
    boost::filesystem::permissions(*path, boost::filesystem::perms::add_perms | requiredPermissions);

    if (boost::filesystem::is_regular_file(*path)) {
        return;
    }

    for (const auto& entry : boost::filesystem::recursive_directory_iterator(*path)) {
        if (!libsarus::filesystem::isSymlink(entry.path())) {
            boost::filesystem::permissions(entry.path(), boost::filesystem::perms::add_perms | requiredPermissions);
        }
    }
}

}
