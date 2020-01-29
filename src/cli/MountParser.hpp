/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_cli_CustomMounts_hpp
#define sarus_cli_CustomMounts_hpp

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <boost/format.hpp>

#include "common/Config.hpp"
#include "common/Logger.hpp"
#include "runtime/Mount.hpp"


namespace sarus {
namespace cli {

class MountParser {
public:
    MountParser(bool isUserMount, std::shared_ptr<const common::Config> conf);
    std::unique_ptr<runtime::Mount> parseMountRequest(const std::unordered_map<std::string, std::string>& mountRequest);

private:
    struct ValidationSettings {
        std::vector<std::string> destinationDisallowedWithPrefix;
        std::vector<std::string> destinationDisallowedExact;
        std::vector<std::string> sourceDisallowedWithPrefix;
        std::vector<std::string> sourceDisallowedExact;
        std::unordered_map<std::string, bool> allowedFlags;
    };

private:
    std::unique_ptr<runtime::Mount> parseBindMountRequest(const std::unordered_map<std::string, std::string>& requestMap);
    unsigned long convertBindMountFlags(const std::unordered_map<std::string, std::string>& requestMap);
    boost::filesystem::path getValidatedMountSource(const std::unordered_map<std::string, std::string>& requestMap);
    boost::filesystem::path getValidatedMountDestination(const std::unordered_map<std::string, std::string>& requestMap);
    std::string convertRequestMapToString(const std::unordered_map<std::string, std::string>&) const;

private:
    std::shared_ptr<const common::Config> conf;
    ValidationSettings validationSettings = {};
};

} // namespace
} // namespace

#endif
