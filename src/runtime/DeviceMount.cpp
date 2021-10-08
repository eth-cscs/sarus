/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "DeviceMount.hpp"

#include <sys/sysmacros.h>

#include "common/Error.hpp"
#include "common/Utility.hpp"
#include "runtime/Utility.hpp"


namespace sarus {
namespace runtime {

DeviceMount::DeviceMount(const boost::filesystem::path& source,
                         const boost::filesystem::path& destination,
                         const unsigned long mountFlags,
                         const common::DeviceAccess& access,
                         std::shared_ptr<const common::Config> config)
    : Mount{source, destination, mountFlags, std::move(config)}
    , access{access}
{
    runtime::utility::logMessage(
            boost::format("Constructing device mount object: source = %s; destination = %s; mount flags = %d; access = %s")
            % source.string() % destination.string() % mountFlags % access.string(), common::LogLevel::DEBUG);

    if (!common::isDeviceFile(source)) {
        auto message = boost::format("Source path '%s' is not a device file")
            % source;
        SARUS_THROW_ERROR(message.str());
    }

    id = common::getDeviceID(source);
    type = common::getDeviceType(source);
}

unsigned int DeviceMount::getMajorID() const {
    return major(id);
}

unsigned int DeviceMount::getMinorID() const {
    return minor(id);
}

} // namespace
} // namespace
