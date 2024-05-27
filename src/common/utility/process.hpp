/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _common_utility_System_hpp
#define _common_utility_System_hpp

#include <string>
#include <vector>

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

#include "common/Config.hpp"
#include "common/UserIdentity.hpp"

/**
 * Utility functions for system operations 
 */

namespace sarus {
namespace common {

void switchIdentity(const common::UserIdentity&);
void setFilesystemUid(const common::UserIdentity&);
std::string executeCommand(const std::string& command);
int forkExecWait(const common::CLIArguments& args,
                 const boost::optional<std::function<void()>>& preExecChildActions = {},
                 const boost::optional<std::function<void(int)>>& postForkParentActions = {},
                 std::iostream* const childStdoutStream= nullptr);
std::string getHostname();
std::vector<int> getCpuAffinity();
void setCpuAffinity(const std::vector<int>&);
void setStdinEcho(bool);

} // namespace
} // namespace

#endif
