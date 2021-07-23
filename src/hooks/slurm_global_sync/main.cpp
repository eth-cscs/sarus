/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "hooks/common/Utility.hpp"
#include "Hook.hpp"


int main(int argc, char* argv[]) {
    try {
        auto hook = sarus::hooks::slurm_global_sync::Hook{};
        hook.dropPrivileges();
        hook.loadConfigs();
        hook.performSynchronization();
    } catch(const sarus::common::Error& e) {
        sarus::common::Logger::getInstance().logErrorTrace(e, "SLURM global sync hook");
        exit(EXIT_FAILURE);
    }
    return 0;
}