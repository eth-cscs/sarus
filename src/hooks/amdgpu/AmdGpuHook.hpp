/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_amdgpu_AmdGpuHook_hpp
#define sarus_hooks_amdgpu_AmdGpuHook_hpp

#include <vector>
#include <unordered_map>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <sys/types.h>

#include "common/LogLevel.hpp"
#include "common/PathHash.hpp"

namespace sarus {
namespace hooks {
namespace amdgpu {

class AmdGpuHook {
public:
    AmdGpuHook();
    void activate();

private:
    void parseConfigJSONOfBundle();
    void bindMountDevices() const;
    void validatedBindMount(const boost::filesystem::path& from, const boost::filesystem::path& to, unsigned long flags) const;
    void log(const std::string& message, sarus::common::LogLevel level) const;
    void log(const boost::format& message, sarus::common::LogLevel level) const;

private:
    boost::filesystem::path bundleDir;
    boost::filesystem::path rootfsDir;
    pid_t pidOfContainer;
    uid_t uidOfUser;
    gid_t gidOfUser;
};

}}} // namespace

#endif
