/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_runtime_Mount_hpp
#define sarus_runtime_Mount_hpp

#include <memory>
#include <boost/filesystem.hpp>
#include <sys/mount.h>

namespace sarus {

// forward declaration to avoid cyclic dependency of headers
namespace common { class Config; }

namespace runtime {

class Mount {
public:
    Mount(  const boost::filesystem::path& source,
            const boost::filesystem::path& destination,
            const unsigned long mountFlags,
            std::shared_ptr<const common::Config> config);

    void performMount() const;

public: // public for test purpose
    boost::filesystem::path source;
    boost::filesystem::path destination;
    unsigned long mountFlags;

private:
    std::weak_ptr<const common::Config> config_weak;
};

}} // namespaces

#endif
