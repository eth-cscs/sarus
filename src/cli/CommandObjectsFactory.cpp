/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "CommandObjectsFactory.hpp"

#include "cli/Utility.hpp"
#include "cli/CommandHelp.hpp"
#include "cli/CommandHelpOfCommand.hpp"
#include "cli/CommandHooks.hpp"
#include "cli/CommandImages.hpp"
#include "cli/CommandPs.hpp"
#include "cli/CommandLoad.hpp"
#include "cli/CommandPull.hpp"
#include "cli/CommandRmi.hpp"
#include "cli/CommandRun.hpp"
#include "cli/CommandSshKeygen.hpp"
#include "cli/CommandKill.hpp"
#include "cli/CommandVersion.hpp"


namespace sarus {
namespace cli {

CommandObjectsFactory::CommandObjectsFactory() {
    addCommand<cli::CommandHelp>("help");
    addCommand<cli::CommandHooks>("hooks");
    addCommand<cli::CommandImages>("images");
    addCommand<cli::CommandLoad>("load");
    addCommand<cli::CommandPs>("ps");
    addCommand<cli::CommandPull>("pull");
    addCommand<cli::CommandRmi>("rmi");
    addCommand<cli::CommandRun>("run");
    addCommand<cli::CommandSshKeygen>("ssh-keygen");
    addCommand<cli::CommandKill>("kill");
    addCommand<cli::CommandVersion>("version");
}

bool CommandObjectsFactory::isValidCommandName(const std::string& commandName) const {
    return map.find(commandName) != map.cend();
}

std::vector<std::string> CommandObjectsFactory::getCommandNames() const {
    auto names = std::vector<std::string>{};
    names.reserve(map.size());
    for(const auto& kv : map) {
        names.push_back(kv.first);
    }
    return names;
}

std::unique_ptr<cli::Command> CommandObjectsFactory::makeCommandObject(const std::string& commandName) const {
    if(!isValidCommandName(commandName)) {
        auto message = boost::format("'%s' is not a Sarus command\nSee 'sarus help'")
            % commandName;
        utility::printLog(message, libsarus::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
    }
    auto it = map.find(commandName);
    return it->second();
}

std::unique_ptr<cli::Command> CommandObjectsFactory::makeCommandObject(
    const std::string& commandName,
    const libsarus::CLIArguments& commandArgs,
    std::shared_ptr<common::Config> config) const {
    if(!isValidCommandName(commandName)) {
        auto message = boost::format("'%s' is not a Sarus command\nSee 'sarus help'")
            % commandName;
        utility::printLog(message, libsarus::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
    }
    auto it = mapWithArguments.find(commandName);
    return it->second(commandArgs, std::move(config));
}

std::unique_ptr<cli::Command> CommandObjectsFactory::makeCommandObjectHelpOfCommand(const std::string& commandName) const {
    auto commandObject = makeCommandObject(commandName);
    auto ptr = new cli::CommandHelpOfCommand{std::move(commandObject)};
    return std::unique_ptr<cli::Command>{ptr};
}

}
}
