/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_CommandStop_hpp
#define cli_CommandStop_hpp

#include "cli/Command.hpp"
#include "cli/HelpMessage.hpp"
#include "cli/Utility.hpp"

#include <runtime/Runtime.hpp>
#include <runtime/Utility.hpp>

#include <boost/format.hpp>
#include <boost/program_options.hpp>

namespace sarus {
namespace cli {

class CommandKill : public Command {
public:
  CommandKill() { }

  CommandKill(const libsarus::CLIArguments &args, std::shared_ptr<common::Config> conf)
      : conf{std::move(conf)} {
    parseCommandArguments(args);
  }

  void execute() override {
    libsarus::logMessage(boost::format("kill container: %s") % containerName, libsarus::LogLevel::INFO);

    auto runcPath = conf->json["runcPath"].GetString();
    auto args = libsarus::CLIArguments{runcPath, 
                                       "--root", "/run/runc/" + std::to_string(conf->userIdentity.uid),
                                       "kill", containerName, "SIGHUP"};

    // execute runc
    auto status = libsarus::forkExecWait(args);

    if (status != 0) {
      auto message = boost::format("%s exited with code %d") % args % status;
      libsarus::logMessage(message, libsarus::LogLevel::WARN);
      exit(status);
    }
  };

  bool requiresRootPrivileges() const override { return true; };
  std::string getBriefDescription() const override { return "Kill a running container"; };
  void printHelpMessage() const override {
    auto printer = cli::HelpMessage()
                       .setUsage("sarus kill [NAME]\n")
                       .setDescription(getBriefDescription());
    std::cout << printer;
  };

private:

  void parseCommandArguments(const libsarus::CLIArguments &args) {
    cli::utility::printLog("parsing CLI arguments of kill command", libsarus::LogLevel::DEBUG);

    libsarus::CLIArguments nameAndOptionArgs, positionalArgs;
    std::tie(nameAndOptionArgs, positionalArgs) = cli::utility::groupOptionsAndPositionalArguments(args, boost::program_options::options_description{});

    // the kill command expects exactly one positional argument (the container name)
    cli::utility::validateNumberOfPositionalArguments(positionalArgs, 1, 1, "kill");

    try {
      containerName = positionalArgs.argv()[0];
    } catch (std::exception &e) {
      auto message = boost::format("%s\nSee 'sarus help kill'") % e.what();
      cli::utility::printLog(message, libsarus::LogLevel::GENERAL, std::cerr);
      SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
    }
  }

  std::string containerName;
  std::shared_ptr<common::Config> conf;
};

} // namespace cli
} // namespace sarus

#endif