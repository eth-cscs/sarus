/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Error.hpp"

namespace libsarus {

std::string getExceptionTypeString(const std::exception& e) {
    auto ret = std::string("generic exception");
    if (dynamic_cast<const std::logic_error*>(&e)) {
        ret = std::string("logic error");
    }
    else if (dynamic_cast<const std::runtime_error*>(&e)) {
        ret = std::string("runtime error");
    }
    else if (dynamic_cast<const std::system_error*>(&e)) {
        ret = std::string("system error");
    }
    else if (dynamic_cast<const std::ios_base::failure*>(&e)) {
        ret = std::string("ios_base failure");
    }
    return ret;
}

}
