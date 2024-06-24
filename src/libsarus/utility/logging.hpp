/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_utility_logging_hpp
#define libsarus_utility_logging_hpp

#include <string>

#include <boost/format.hpp>

#include "libsarus/Logger.hpp"

/**
 * Utility functions for output controls 
 */

namespace libsarus {

void logMessage(const std::string&, LogLevel, std::ostream& out = std::cout, std::ostream& err = std::cerr);
void logMessage(const boost::format&, LogLevel, std::ostream& out = std::cout, std::ostream& err = std::cerr);

}

#endif
