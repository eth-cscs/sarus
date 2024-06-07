/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_mpi_SharedLibrary_hpp
#define sarus_hooks_mpi_SharedLibrary_hpp

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
    bool isFullAbiCompatible(const SharedLibrary& hostLibrary) const;
    bool isMajorAbiCompatible(const SharedLibrary& hostLibrary) const;
    SharedLibrary pickNewestAbiCompatibleLibrary(const std::vector<SharedLibrary>& candidates) const;
    std::string getLinkerName() const;
    boost::filesystem::path getPath() const;
    std::string getRealName() const;
    const boost::optional<int>& getMajorVersion() const { return major; }
    const boost::optional<int>& getMinorVersion() const { return minor; }

private:
    boost::optional<int> major;
    boost::optional<int> minor;
    boost::optional<int> patch;
    std::string linkerName;
    boost::filesystem::path path;
    std::string realName;
};

bool areMajorAbiCompatible(const SharedLibrary&, const SharedLibrary&);
bool areFullAbiCompatible(const SharedLibrary&, const SharedLibrary&);
bool areStrictlyAbiCompatible(const SharedLibrary&, const SharedLibrary&);

}}}

#endif
