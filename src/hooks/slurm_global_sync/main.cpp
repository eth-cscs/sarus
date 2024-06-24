/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "libsarus/Error.hpp"
#include "libsarus/Logger.hpp"
#include "Hook.hpp"


int main(int argc, char* argv[]) {
    try {
        auto hook = sarus::hooks::slurm_global_sync::Hook{};
        hook.dropPrivileges();
        hook.loadConfigs();
        hook.performSynchronization();
    } catch(const libsarus::Error& e) {
        libsarus::Logger::getInstance().logErrorTrace(e, "SLURM global sync hook");
        exit(EXIT_FAILURE);
    }
    return 0;
}