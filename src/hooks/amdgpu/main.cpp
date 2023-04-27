/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "AmdGpuHook.hpp"
#include "common/Error.hpp"
#include "common/Logger.hpp"

int main(int argc, char* argv[]) {
  try {
    sarus::hooks::amdgpu::AmdGpuHook{}.activate();
  } catch (const sarus::common::Error& e) {
    sarus::common::Logger::getInstance().logErrorTrace(e, "AMD GPU hook");
    exit(EXIT_FAILURE);
  }
  return 0;
}
