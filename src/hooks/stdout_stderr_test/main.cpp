/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <iostream>

#include "hooks/common/Utility.hpp"

using namespace sarus;

int main(int argc, char* argv[]) {
    hooks::common::utility::useSarusStdoutStderrIfAvailable();

    std::cout << "hook's stdout" << std::endl;
    std::cerr << "hook's stderr" << std::endl;

    return 0;
}
