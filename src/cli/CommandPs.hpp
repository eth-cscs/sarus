/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_CommandPs_hpp
#define cli_CommandPs_hpp

#include "cli/Command.hpp"
#include "cli/HelpMessage.hpp"
#include "cli/Utility.hpp"

#include <runtime/Runtime.hpp>
#include <runtime/Utility.hpp>

#include <boost/format.hpp>
#include <boost/program_options.hpp>

namespace sarus {
namespace cli {

class CommandPs : public Command {
public:
  CommandPs() {}

  CommandPs(const libsarus::CLIArguments &args, std::shared_ptr<common::Config> conf)
      : conf{std::move(conf)}
  {}

  void execute() override {
    auto runcPath = conf->json["runcPath"].GetString();
    auto args = libsarus::CLIArguments{runcPath, 
                                       "--root", "/run/runc/" + std::to_string(conf->userIdentity.uid),
                                       "list"};

    // execute runc
    auto status = libsarus::forkExecWait(args);

    if (status != 0) {
      auto message = boost::format("%s exited with code %d") % args % status;
      libsarus::logMessage(message, libsarus::LogLevel::WARN);
      exit(status);
    }

  };

  bool requiresRootPrivileges() const override { return true; };
  std::string getBriefDescription() const override { return "List running containers"; }

  void printHelpMessage() const override {
    auto printer = cli::HelpMessage()
                       .setUsage("sarus ps\n")
                       .setDescription(getBriefDescription());
    std::cout << printer;
  };

  private:
  std::shared_ptr<common::Config> conf;
};

} // namespace cli
} // namespace sarus

#endif