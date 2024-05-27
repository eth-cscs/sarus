/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _common_utility_Logging_hpp
#define _common_utility_Logging_hpp

#include <string>

#include <boost/format.hpp>

#include "common/Logger.hpp"

/**
 * Utility functions for output controls 
 */

namespace sarus {
namespace common {

void logMessage(const std::string&, LogLevel, std::ostream& out = std::cout, std::ostream& err = std::cerr);
void logMessage(const boost::format&, LogLevel, std::ostream& out = std::cout, std::ostream& err = std::cerr);

} // namespace
} // namespace

#endif
