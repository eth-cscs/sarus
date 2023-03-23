/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_cli_DeviceParser_hpp
#define sarus_cli_DeviceParser_hpp

#include <string>
#include <vector>

#include "common/UserIdentity.hpp"
#include "runtime/DeviceMount.hpp"


namespace sarus {
namespace cli {

class DeviceParser {
public:
    DeviceParser(const boost::filesystem::path& rootfsDir, const common::UserIdentity& userIdentity);
    DeviceParser(std::shared_ptr<const common::Config> conf);
    std::unique_ptr<runtime::DeviceMount> parseDeviceRequest(const std::string& requestString) const;

private:
    common::DeviceAccess createDeviceAccess(const std::string& accessString) const;
    void validateMountPath(const boost::filesystem::path& path, const std::string& context) const;

private:
    boost::filesystem::path rootfsDir;
    common::UserIdentity userIdentity;
};

} // namespace
} // namespace

#endif
