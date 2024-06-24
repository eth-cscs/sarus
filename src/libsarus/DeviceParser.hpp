/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_DeviceParser_hpp
#define libsarus_DeviceParser_hpp

#include <string>
#include <vector>

#include "libsarus/UserIdentity.hpp"
#include "libsarus/DeviceMount.hpp"

namespace libsarus {

class DeviceParser {
public:
    DeviceParser(const boost::filesystem::path& rootfsDir, const libsarus::UserIdentity& userIdentity);
    std::unique_ptr<libsarus::DeviceMount> parseDeviceRequest(const std::string& requestString) const;

private:
    libsarus::DeviceAccess createDeviceAccess(const std::string& accessString) const;
    void validateMountPath(const boost::filesystem::path& path, const std::string& context) const;

private:
    boost::filesystem::path rootfsDir;
    libsarus::UserIdentity userIdentity;
};

}

#endif
