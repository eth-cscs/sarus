/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/CLIArguments.hpp"
#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "MountHook.hpp"

int main(int argc, char* argv[]) {
    try {
        auto args = sarus::common::CLIArguments(argc, argv);
        sarus::hooks::mount::MountHook{args}.activate();
    } catch(const sarus::common::Error& e) {
        sarus::common::Logger::getInstance().logErrorTrace(e, "Mount hook");
        exit(EXIT_FAILURE);
    }
    return 0;
}
