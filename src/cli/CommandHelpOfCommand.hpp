/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_CommandHelpOfCommand_hpp
#define cli_CommandHelpOfCommand_hpp

#include <stdexcept>

#include <boost/format.hpp>

#include "cli/Utility.hpp"
#include "common/Config.hpp"
#include "cli/Command.hpp"

namespace sarus {
namespace cli {

class CommandHelpOfCommand : public Command {
public:
    CommandHelpOfCommand() = default;

    CommandHelpOfCommand(std::unique_ptr<cli::Command> command)
        : command(std::move(command))
    {}
    
    void execute() override {
        command->printHelpMessage();
    }

    bool requiresRootPrivileges() const override {
        return false;
    }

    std::string getBriefDescription() const override {
        SARUS_THROW_ERROR("This function must not be executed."
                            " The developer should review the program's logic.");
    }

    void printHelpMessage() const override {
        SARUS_THROW_ERROR("This function must not be executed."
                            " The developer should review the program's logic.");
    }

private:
    std::unique_ptr<cli::Command> command;
};

}
}

#endif
