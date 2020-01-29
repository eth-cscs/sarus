/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_TimestampHook_hpp
#define sarus_hooks_TimestampHook_hpp

#include <boost/filesystem.hpp>
#include <string>


namespace sarus {
namespace hooks {
namespace timestamp {

class TimestampHook {
public:
    void activate();

private:
    void parseConfigJSONOfBundle();
    void parseEnvironmentVariables();
    void timestamp();

private:
    bool isHookEnabled{ false };
    std::string message;
    boost::filesystem::path logFilePath;
    boost::filesystem::path bundleDir;
    pid_t pidOfContainer;
    uid_t uidOfUser;
    gid_t gidOfUser;
};

}}} // namespace

#endif
