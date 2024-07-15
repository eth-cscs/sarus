/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "DeviceMount.hpp"

#include <sys/sysmacros.h>

#include "libsarus/Error.hpp"
#include "libsarus/Utility.hpp"

namespace libsarus {

DeviceMount::DeviceMount(Mount&& baseMount, const DeviceAccess& access)
    : Mount{std::move(baseMount)}
    , access{access}
{
    logMessage(boost::format("Constructing device mount object: source = %s; destination = %s; mount flags = %d; access = %s")
            % getSource().string() % getDestination().string() % getFlags() % access.string(), LogLevel::DEBUG);

    if (!libsarus::filesystem::isDeviceFile(getSource())) {
        auto message = boost::format("Source path %s is not a device file") % getSource();
        SARUS_THROW_ERROR(message.str());
    }

    id = libsarus::filesystem::getDeviceID(getSource());
    type = libsarus::filesystem::getDeviceType(getSource());
}

unsigned int DeviceMount::getMajorID() const {
    return major(id);
}

unsigned int DeviceMount::getMinorID() const {
    return minor(id);
}

}
