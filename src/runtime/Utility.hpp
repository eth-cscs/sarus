/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _runtime_Utility_hpp
#define _runtime_Utility_hpp

#include <vector>

#include <boost/format.hpp>

#include "common/Config.hpp"
#include "libsarus/Logger.hpp"
#include "libsarus/Mount.hpp"

namespace sarus {
namespace runtime {
namespace utility {

void setupSignalProxying(const pid_t childPid);
std::vector<std::unique_ptr<libsarus::Mount>> generatePMIxMounts(std::shared_ptr<const common::Config>);
void logMessage(const boost::format&, libsarus::LogLevel,
                std::ostream& out=std::cout, std::ostream& err=std::cerr);
void logMessage(const std::string&, libsarus::LogLevel,
                std::ostream& out=std::cout, std::ostream& err=std::cerr);

} // namespace
} // namespace
} // namespace

#endif
