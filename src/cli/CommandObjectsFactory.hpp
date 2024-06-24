/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_CommandObjectsFactory_hpp
#define cli_CommandObjectsFactory_hpp

#include <string>
#include <vector>
#include <deque>
#include <memory>

#include "common/Config.hpp"
#include "libsarus/CLIArguments.hpp"
#include "cli/Command.hpp"


namespace sarus {
namespace cli {

class CommandObjectsFactory {
public:
    CommandObjectsFactory();

    template<class CommandType>
    void addCommand(const std::string& commandName) {
        map[commandName] = []() {
            return std::unique_ptr<cli::Command>{new CommandType{}};
        };
        mapWithArguments[commandName] = [](const libsarus::CLIArguments& commandArgs, std::shared_ptr<common::Config> config) {
            return std::unique_ptr<cli::Command>{new CommandType{commandArgs, std::move(config)}};
        };
    }

    bool isValidCommandName(const std::string& commandName) const;
    std::vector<std::string> getCommandNames() const;
    std::unique_ptr<cli::Command> makeCommandObject(const std::string& commandName) const;
    std::unique_ptr<cli::Command> makeCommandObject(const std::string& commandName,
                                                    const libsarus::CLIArguments& commandArgs,
                                                    std::shared_ptr<common::Config> config) const;
    std::unique_ptr<cli::Command> makeCommandObjectHelpOfCommand(const std::string& commandName) const;

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<cli::Command>()>> map;
    std::unordered_map<std::string, std::function<std::unique_ptr<cli::Command>(
        const libsarus::CLIArguments&,
        std::shared_ptr<common::Config>)>> mapWithArguments;
};

}
}

#endif
