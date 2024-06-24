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
#include "MpiHook.hpp"

int main(int argc, char* argv[]) {
    try {
        sarus::hooks::mpi::MpiHook{}.activateMpiSupport();
    } catch(const libsarus::Error& e) {
        libsarus::Logger::getInstance().logErrorTrace(e, "MPI hook");
        exit(EXIT_FAILURE);
    }
    return 0;
}
