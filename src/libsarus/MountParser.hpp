/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_MountParser_hpp
#define libsarus_MountParser_hpp

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <boost/format.hpp>
#include <rapidjson/document.h>

#include "libsarus/Logger.hpp"
#include "libsarus/Mount.hpp"

namespace libsarus {

class MountParser {
public:
    MountParser(const boost::filesystem::path& rootfsDir, const libsarus::UserIdentity& userIdentity);
    void setMountDestinationRestrictions(const rapidjson::Value& destinationRestrictions);
    std::unique_ptr<libsarus::Mount> parseMountRequest(const std::unordered_map<std::string, std::string>& mountRequest);

private:
    struct ValidationSettings {
        std::vector<std::string> destinationDisallowedWithPrefix;
        std::vector<std::string> destinationDisallowedExact;
        std::vector<std::string> sourceDisallowedWithPrefix;
        std::vector<std::string> sourceDisallowedExact;
    };

private:
    std::unique_ptr<libsarus::Mount> parseBindMountRequest(const std::unordered_map<std::string, std::string>& requestMap);
    unsigned long convertBindMountFlags(const std::unordered_map<std::string, std::string>& requestMap);
    boost::filesystem::path getValidatedMountSource(const std::unordered_map<std::string, std::string>& requestMap);
    boost::filesystem::path getValidatedMountDestination(const std::unordered_map<std::string, std::string>& requestMap);
    std::string convertRequestMapToString(const std::unordered_map<std::string, std::string>&) const;

private:
    ValidationSettings validationSettings = {};
    boost::filesystem::path rootfsDir;
    libsarus::UserIdentity userIdentity;
};

}

#endif
