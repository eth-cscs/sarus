/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/**
 * @brief Utility functions to be used in the tests.
 */

#ifndef sarus_test_utility_config_hpp
#define sarus_test_utility_config_hpp

#include <memory>

#include "common/Config.hpp"

namespace test_utility {
namespace config {

struct ConfigRAII {
    ~ConfigRAII();
    std::shared_ptr<sarus::common::Config> config;
};

ConfigRAII makeConfig();

}
}

#endif
