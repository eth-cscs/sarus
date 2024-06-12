/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "GlibcHook.hpp"

int main(int argc, char* argv[]) {
    try {
        sarus::hooks::glibc::GlibcHook{}.injectGlibcLibrariesIfNecessary();
    } catch(const sarus::common::Error& e) {
        sarus::common::Logger::getInstance().logErrorTrace(e, "Glibc hook");
        exit(EXIT_FAILURE);
    }
    return 0;
}
