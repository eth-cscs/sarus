/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_cli_MountParser_hpp
#define sarus_cli_MountParser_hpp

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
    MountParser(const bool isUserMount, std::shared_ptr<const common::Config> conf);
    MountParser(const boost::filesystem::path& rootfsDir, const common::UserIdentity& userIdentity);
    std::unique_ptr<runtime::Mount> parseMountRequest(const std::unordered_map<std::string, std::string>& mountRequest);

private:
    struct ValidationSettings {
        std::vector<std::string> destinationDisallowedWithPrefix;
        std::vector<std::string> destinationDisallowedExact;
        std::vector<std::string> sourceDisallowedWithPrefix;
        std::vector<std::string> sourceDisallowedExact;
    };

private:
    std::unique_ptr<runtime::Mount> parseBindMountRequest(const std::unordered_map<std::string, std::string>& requestMap);
    unsigned long convertBindMountFlags(const std::unordered_map<std::string, std::string>& requestMap);
    boost::filesystem::path getValidatedMountSource(const std::unordered_map<std::string, std::string>& requestMap);
    boost::filesystem::path getValidatedMountDestination(const std::unordered_map<std::string, std::string>& requestMap);
    std::string convertRequestMapToString(const std::unordered_map<std::string, std::string>&) const;

private:
    ValidationSettings validationSettings = {};
    boost::filesystem::path rootfsDir;
    common::UserIdentity userIdentity;
};

} // namespace
} // namespace

#endif
