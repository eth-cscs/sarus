/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_cli_DeviceParser_hpp
#define sarus_cli_DeviceParser_hpp

#include <string>
#include <vector>

#include "common/Config.hpp"
#include "runtime/DeviceMount.hpp"


namespace sarus {
namespace cli {

class DeviceParser {
public:
    DeviceParser(std::shared_ptr<const common::Config> conf);
    std::unique_ptr<runtime::DeviceMount> parseDeviceRequest(const std::string& requestString) const ;

private:
    common::DeviceAccess createDeviceAccess(const std::string& accessString) const;
    void validateMountPath(const boost::filesystem::path& path, const std::string& context) const;

private:
    std::shared_ptr<const common::Config> conf;
};

} // namespace
} // namespace

#endif
