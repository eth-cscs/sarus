/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string>
#include <iostream>


int main(int argc, char* argv[]) {
    if (argc > 1 && std::string{argv[1]} == std::string{"--version"}) {
        std::cout << "ldd (GNU libc) 2.25\n"
                     "Copyright (C) 2021 Free Software Foundation, Inc.\n"
                     "This is free software; see the source for copying conditions.  There is NO\n"
                     "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
                     "Written by Roland McGrath and Ulrich Drepper." << std::endl;
    }
    return 0;
}
