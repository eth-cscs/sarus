/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_runtime_DeviceMount_hpp
#define sarus_runtime_DeviceMount_hpp

#include <sys/types.h>

#include "Mount.hpp"
#include "common/DeviceAccess.hpp"


namespace sarus {
namespace runtime {

/**
 * This class represents a bind mount for a device file
 */
class DeviceMount : public Mount {
public:
    DeviceMount(const boost::filesystem::path& source,
                const boost::filesystem::path& destination,
                const unsigned long mountFlags,
                const common::DeviceAccess& access,
                std::shared_ptr<const common::Config> config);

public:
    char getType() const {return type;};
    unsigned int getMajorID() const;
    unsigned int getMinorID() const;
    const common::DeviceAccess& getAccess() const {return access;};

private:
    common::DeviceAccess access;
    dev_t id;
    char type;
};

}} // namespaces

#endif
