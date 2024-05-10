/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_common_DeviceMount_hpp
#define sarus_common_DeviceMount_hpp

#include <sys/types.h>

#include "Mount.hpp"
#include "common/DeviceAccess.hpp"


namespace sarus {
namespace common {

/**
 * This class represents a bind mount for a device file
 */
class DeviceMount : public Mount {
public:
    DeviceMount(Mount&& baseMount, const DeviceAccess& access);

public:
    char getType() const {return type;};
    unsigned int getMajorID() const;
    unsigned int getMinorID() const;
    const DeviceAccess& getAccess() const {return access;};

private:
    DeviceAccess access;
    dev_t id;
    char type;
};

}} // namespaces

#endif
