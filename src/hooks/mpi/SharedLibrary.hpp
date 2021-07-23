/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_mpi_Utils_hpp
#define sarus_hooks_mpi_Utils_hpp

#include <boost/filesystem.hpp>
#include <vector>

// See comment on <sys/types.h> or https://bugzilla.redhat.com/show_bug.cgi?id=130601
#undef major
#undef minor

namespace sarus {
namespace hooks {
namespace mpi {

class SharedLibrary {
    // Using naming convention mentioned in The Linux Programming Interface book.
public:
    SharedLibrary(const boost::filesystem::path& path, const boost::filesystem::path& rootDir="");

    bool hasMajorVersion() const;
    bool isFullAbiCompatible(const SharedLibrary& sl) const;
    bool isMajorAbiCompatible(const SharedLibrary& sl) const;
    SharedLibrary pickNewestAbiCompatibleLibrary(const std::vector<SharedLibrary>& candidates) const;
    std::string getLinkerName() const;
    boost::filesystem::path getPath() const;
    std::string getRealName() const;

private:
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string linkerName;
    boost::filesystem::path path;
    std::string realName;
};

}}}

#endif
