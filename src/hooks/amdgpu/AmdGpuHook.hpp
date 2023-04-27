/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef SRC_HOOKS_AMDGPU_AMDGPUHOOK_HPP_
#define SRC_HOOKS_AMDGPU_AMDGPUHOOK_HPP_

#include <string>

#include <boost/format.hpp>

#include "common/LogLevel.hpp"
#include "common/PathHash.hpp"
#include "common/UserIdentity.hpp"

namespace sarus {
namespace hooks {
namespace amdgpu {

class AmdGpuHook {
 public:
  AmdGpuHook();
  void activate();

 private:
  void parseConfigJSONOfBundle();
  void performBindMounts() const;
  void log(const std::string& message, sarus::common::LogLevel level) const;
  void log(const boost::format& message, sarus::common::LogLevel level) const;

  boost::filesystem::path bundleDir;
  boost::filesystem::path rootfsDir;
  pid_t pidOfContainer;
  sarus::common::UserIdentity userIdentity;
};

}  // namespace amdgpu
}  // namespace hooks
}  // namespace sarus

#endif  // SRC_HOOKS_AMDGPU_AMDGPUHOOK_HPP_
