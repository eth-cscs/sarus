/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _runtime_Utility_hpp
#define _runtime_Utility_hpp

#include <boost/format.hpp>

#include "common/Logger.hpp"

namespace sarus {
namespace runtime {
namespace utility {

void logMessage(const boost::format&, common::LogLevel,
                std::ostream& out=std::cout, std::ostream& err=std::cerr);
void logMessage(const std::string&, common::LogLevel,
                std::ostream& out=std::cout, std::ostream& err=std::cerr);

} // namespace
} // namespace
} // namespace

#endif
