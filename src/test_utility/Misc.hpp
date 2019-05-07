/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_test_utility_Misc_hpp
#define sarus_test_utility_Misc_hpp

#include <boost/regex.hpp>

#include "common/Error.hpp"
#include "common/Utility.hpp"


namespace test_utility {
namespace misc {

std::tuple<uid_t, gid_t> getNonRootUserIds() {
    auto out = sarus::common::executeCommand("getent passwd");
    std::stringstream ss{out};
    std::string line;

    boost::smatch matches;
    boost::regex pattern("^.+:.*:([0-9]+):([0-9]+):.*:.*:.*$");

    while(std::getline(ss, line)) {
        if(!boost::regex_match(line, matches, pattern)) {
            SARUS_THROW_ERROR("Failed to find non-root user ids. Regex pattern is invalid.");
        }
        auto uid = std::stoi(std::string(matches[1].first, matches[1].second));
        auto gid = std::stoi(std::string(matches[2].first, matches[2].second));
        if(uid != 0) {
            return std::tuple<uid_t, gid_t>{uid, gid};
        }
    }

    SARUS_THROW_ERROR("Failed to find non-root user ids");
}

}
}

#endif
