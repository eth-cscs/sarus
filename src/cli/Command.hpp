/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_command_hpp
#define cli_command_hpp

#include "common/Config.hpp"

namespace sarus {
namespace cli {

class Command {
public:
    virtual ~Command() {}
    virtual void execute() = 0;
    virtual bool requiresRootPrivileges() const = 0;
    virtual std::string getBriefDescription() const = 0;
    virtual void printHelpMessage() const = 0;
};

}
}

#endif
