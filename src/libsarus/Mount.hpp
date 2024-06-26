/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_Mount_hpp
#define libsarus_Mount_hpp

#include <memory>
#include <boost/filesystem.hpp>
#include <sys/mount.h>

#include "libsarus/UserIdentity.hpp"

namespace libsarus {

class Mount {
public:
    Mount(const boost::filesystem::path& source,
          const boost::filesystem::path& destination,
          const unsigned long mountFlags,
          const boost::filesystem::path& rootfsDir,
          const libsarus::UserIdentity userIdentity);

    void performMount() const;

    boost::filesystem::path getSource() const { return source; };
    boost::filesystem::path getDestination() const { return destination; };
    unsigned long getFlags() const { return mountFlags; };

private:
    boost::filesystem::path source;
    boost::filesystem::path destination;
    unsigned long mountFlags;
    boost::filesystem::path rootfsDir;
    libsarus::UserIdentity userIdentity;
};

}

#endif
