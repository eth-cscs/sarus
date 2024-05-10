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

#include "common/Error.hpp"
#include "common/Utility.hpp"



namespace sarus {
namespace common {

DeviceMount::DeviceMount(Mount&& baseMount, const DeviceAccess& access)
    : Mount{std::move(baseMount)}
    , access{access}
{
    logMessage(boost::format("Constructing device mount object: source = %s; destination = %s; mount flags = %d; access = %s")
            % source.string() % destination.string() % mountFlags % access.string(), LogLevel::DEBUG);

    if (!isDeviceFile(source)) {
        auto message = boost::format("Source path %s is not a device file")
            % source;
        SARUS_THROW_ERROR(message.str());
    }

    id = getDeviceID(source);
    type = getDeviceType(source);
}

unsigned int DeviceMount::getMajorID() const {
    return major(id);
}

unsigned int DeviceMount::getMinorID() const {
    return minor(id);
}

} // namespace
} // namespace
