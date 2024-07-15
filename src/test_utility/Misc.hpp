/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_test_utility_Misc_hpp
#define sarus_test_utility_Misc_hpp

#include <sstream>

#include <rapidjson/prettywriter.h>

#include "libsarus/Error.hpp"
#include "libsarus/Utility.hpp"
#include "libsarus/PasswdDB.hpp"


namespace test_utility {
namespace misc {

std::tuple<uid_t, gid_t> getNonRootUserIds() {
    auto out = libsarus::process::executeCommand("getent passwd");
    std::stringstream ss{out};
    auto passwd = libsarus::PasswdDB{ss};

    for(const auto& entry : passwd.getEntries()) {
        if(entry.uid != 0) {
            return std::tuple<uid_t, gid_t>{entry.uid, entry.gid};
        }
    }
    
    SARUS_THROW_ERROR("Failed to find non-root user ids");
}

std::string prettyPrintJSON(const rapidjson::Value& json) {
    namespace rj = rapidjson;
    rj::StringBuffer buffer;
    rj::PrettyWriter<rj::StringBuffer> writer(buffer);
    json.Accept(writer);
    return buffer.GetString();
}

}
}

#endif
