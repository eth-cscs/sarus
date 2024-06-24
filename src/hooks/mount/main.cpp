/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "libsarus/CLIArguments.hpp"
#include "libsarus/Error.hpp"
#include "libsarus/Logger.hpp"
#include "MountHook.hpp"

int main(int argc, char* argv[]) {
    try {
        auto args = libsarus::CLIArguments(argc, argv);
        sarus::hooks::mount::MountHook{args}.activate();
    } catch(const libsarus::Error& e) {
        libsarus::Logger::getInstance().logErrorTrace(e, "Mount hook");
        exit(EXIT_FAILURE);
    }
    return 0;
}
