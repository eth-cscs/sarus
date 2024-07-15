/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_utility_system_hpp
#define libsarus_utility_system_hpp

#include <string>
#include <vector>

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

#include "libsarus/CLIArguments.hpp"
#include "libsarus/UserIdentity.hpp"

/**
 * Utility functions for system operations 
 */

namespace libsarus {
namespace process {

void switchIdentity(const libsarus::UserIdentity&);
void setFilesystemUid(const libsarus::UserIdentity&);
std::string executeCommand(const std::string& command);
int forkExecWait(const libsarus::CLIArguments& args,
                 const boost::optional<std::function<void()>>& preExecChildActions = {},
                 const boost::optional<std::function<void(int)>>& postForkParentActions = {},
                 std::iostream* const childStdoutStream= nullptr);
std::string getHostname();
std::vector<int> getCpuAffinity();
void setCpuAffinity(const std::vector<int>&);
void setStdinEcho(bool);

}}

#endif
