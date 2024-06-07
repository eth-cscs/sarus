/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_common_Mount_hpp
#define sarus_common_Mount_hpp

#include <memory>
#include <boost/filesystem.hpp>
#include <sys/mount.h>

#include "common/UserIdentity.hpp"


namespace sarus {
namespace common {

// forward declaration to avoid cyclic dependency of headers
class Config;

class Mount {
public:
    Mount(const boost::filesystem::path& source,
          const boost::filesystem::path& destination,
          const unsigned long mountFlags,
          const boost::filesystem::path& rootfsDir,
          const common::UserIdentity userIdentity);
    Mount(const boost::filesystem::path& source,
          const boost::filesystem::path& destination,
          const unsigned long mountFlags,
          std::shared_ptr<const common::Config> config);

    void performMount() const;

public: // public for test purpose
    boost::filesystem::path source;
    boost::filesystem::path destination;
    unsigned long mountFlags;

private:
    boost::filesystem::path rootfsDir;
    common::UserIdentity userIdentity;
};

}} // namespaces

#endif