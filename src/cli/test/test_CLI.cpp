/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <memory>

#include <boost/filesystem.hpp>

#include "common/Utility.hpp"
#include "common/Config.hpp"
#include "common/CLIArguments.hpp"
#include "cli/CommandObjectsFactory.hpp"
#include "cli/CLI.hpp"
#include "cli/CommandHelp.hpp"
#include "cli/CommandHelpOfCommand.hpp"
#include "cli/CommandImages.hpp"
#include "cli/CommandLoad.hpp"
#include "cli/CommandPull.hpp"
#include "cli/CommandRmi.hpp"
#include "cli/CommandRun.hpp"
#include "cli/CommandSshKeygen.hpp"
#include "cli/CommandVersion.hpp"
#include "runtime/Mount.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(CLITestGroup) {
};

std::unique_ptr<cli::Command> generateCommandFromCLIArguments(const common::CLIArguments& args) {
    auto cli = cli::CLI{};
    auto configRAII = test_utility::config::makeConfig();
    return cli.parseCommandLine(args, configRAII.config);
}

template<class ExpectedDynamicType>
void checkCommandDynamicType(const cli::Command& command) {
    CHECK(dynamic_cast<const ExpectedDynamicType*>(&command) != nullptr);
}

TEST(CLITestGroup, LogLevel) {
    auto& logger = common::Logger::getInstance();
    generateCommandFromCLIArguments({"sarus"});
    CHECK(logger.getLevel() == common::LogLevel::WARN);

    generateCommandFromCLIArguments({"sarus", "--verbose"});
    CHECK(logger.getLevel() == common::LogLevel::INFO);

    generateCommandFromCLIArguments({"sarus", "--debug"});
    CHECK(logger.getLevel() == common::LogLevel::DEBUG);
}

TEST(CLITestGroup, CommandTypes) {
    auto command = generateCommandFromCLIArguments({"sarus"});
    checkCommandDynamicType<cli::CommandHelp>(*command);

    command = generateCommandFromCLIArguments({"sarus", "help"});
    checkCommandDynamicType<cli::CommandHelp>(*command);

    command = generateCommandFromCLIArguments({"sarus", "--help"});
    checkCommandDynamicType<cli::CommandHelp>(*command);

    command = generateCommandFromCLIArguments({"sarus", "help", "pull"});
    checkCommandDynamicType<cli::CommandHelpOfCommand>(*command);

    command = generateCommandFromCLIArguments({"sarus", "images"});
    checkCommandDynamicType<cli::CommandImages>(*command);

    command = generateCommandFromCLIArguments({"sarus", "load", "archive.tar", "image"});
    checkCommandDynamicType<cli::CommandLoad>(*command);

    command = generateCommandFromCLIArguments({"sarus", "pull", "image"});
    checkCommandDynamicType<cli::CommandPull>(*command);

    command = generateCommandFromCLIArguments({"sarus", "rmi", "image"});
    checkCommandDynamicType<cli::CommandRmi>(*command);

    command = generateCommandFromCLIArguments({"sarus", "run", "image"});
    checkCommandDynamicType<cli::CommandRun>(*command);

    command = generateCommandFromCLIArguments({"sarus", "ssh-keygen"});
    checkCommandDynamicType<cli::CommandSshKeygen>(*command);

    command = generateCommandFromCLIArguments({"sarus", "version"});
    checkCommandDynamicType<cli::CommandVersion>(*command);

    command = generateCommandFromCLIArguments({"sarus", "--version"});
    checkCommandDynamicType<cli::CommandVersion>(*command);
}

TEST(CLITestGroup, UnrecognizedGlobalOptions) {
    CHECK_THROWS(common::Error, generateCommandFromCLIArguments({"sarus", "--mpi", "run"}));
}

std::shared_ptr<common::Config> generateConfig(const common::CLIArguments& args) {
    auto commandName = args.argv()[0];
    auto cli = cli::CLI{};
    auto argsGroups = cli.groupArgumentsAndCorrespondingOptions(args);

    auto configRAII = test_utility::config::makeConfig();
    auto factory = cli::CommandObjectsFactory{};
    auto command = factory.makeCommandObject(commandName, argsGroups, configRAII.config);
    return configRAII.config;
}

TEST(CLITestGroup, generated_config_for_CommandLoad) {
    auto conf = generateConfig(
        {"load", "--temp-dir=/custom-temp-dir", "archive.tar", "library/image:tag"});
    auto expectedArchivePath = boost::filesystem::absolute("archive.tar");
    CHECK_EQUAL(conf->directories.temp.string(), std::string{"/custom-temp-dir"});
    CHECK_EQUAL(conf->archivePath.string(), expectedArchivePath.string());
    CHECK_EQUAL(conf->imageID.server, std::string{"load"});
    CHECK_EQUAL(conf->imageID.repositoryNamespace, std::string{"library"});
    CHECK_EQUAL(conf->imageID.image, std::string{"image"});
    CHECK_EQUAL(conf->imageID.tag, std::string{"tag"});
}

TEST(CLITestGroup, generated_config_for_CommandPull) {
    auto conf = generateConfig(
        {"pull",
        "--temp-dir=/custom-temp-dir",
        "my.own.server:5000/user/image:tag"});
    CHECK(conf->directories.temp.string() == "/custom-temp-dir");
    CHECK(conf->imageID.server == "my.own.server:5000");
    CHECK(conf->imageID.repositoryNamespace == "user");
    CHECK(conf->imageID.image == "image");
    CHECK(conf->imageID.tag == "tag");
}

TEST(CLITestGroup, generated_config_for_CommandRmi) {
    auto conf = generateConfig({"rmi", "ubuntu"});
    CHECK_EQUAL(conf->imageID.server, std::string{"index.docker.io"});
    CHECK_EQUAL(conf->imageID.repositoryNamespace, std::string{"library"});
    CHECK_EQUAL(conf->imageID.image, std::string{"ubuntu"});
    CHECK_EQUAL(conf->imageID.tag, std::string{"latest"});
}

TEST(CLITestGroup, generated_config_for_CommandRun) {
    {
        auto conf = generateConfig({"run", "image"});
        CHECK_EQUAL(conf->imageID.server, std::string{"index.docker.io"});
        CHECK_EQUAL(conf->imageID.repositoryNamespace, std::string{"library"});
        CHECK_EQUAL(conf->imageID.image, std::string{"image"});
        CHECK_EQUAL(conf->imageID.tag, std::string{"latest"});
        CHECK_EQUAL(conf->commandRun.useMPI, 0);
        CHECK_EQUAL(conf->commandRun.enableGlibcReplacement, 0);
        CHECK(conf->commandRun.execArgs.argc() == 0);
    }
    {
        auto conf = generateConfig({"run",
                                    "--workdir=/workdir",
                                    "--mpi",
                                    "--glibc",
                                    "--mount=type=bind,source=/source,destination=/destination",
                                    "ubuntu", "bash", "-c", "ls /dev |grep nvidia"});
        CHECK_EQUAL(conf->commandRun.workdir->string(), std::string{"/workdir"});
        CHECK_EQUAL(conf->commandRun.useMPI, true);
        CHECK_EQUAL(conf->commandRun.enableGlibcReplacement, true);
        CHECK_EQUAL(conf->commandRun.mounts.size(), 2); // 1 site mount + 1 user mount
        CHECK_EQUAL(conf->imageID.server, std::string{"index.docker.io"});
        CHECK_EQUAL(conf->imageID.repositoryNamespace, std::string{"library"});
        CHECK_EQUAL(conf->imageID.image, std::string{"ubuntu"});
        CHECK_EQUAL(conf->imageID.tag, std::string{"latest"});
        CHECK(conf->commandRun.execArgs.argc() == 3);
        CHECK_EQUAL(conf->commandRun.execArgs.argv()[0], std::string{"bash"});
        CHECK_EQUAL(conf->commandRun.execArgs.argv()[1], std::string{"-c"});
        CHECK_EQUAL(conf->commandRun.execArgs.argv()[2], std::string{"ls /dev |grep nvidia"});
    }
}

std::deque<common::CLIArguments> generateGroupedArguments(const common::CLIArguments& args) {
    auto cli = cli::CLI{};
    return cli.groupArgumentsAndCorrespondingOptions(args);
}

TEST(CLITestGroup, groupArgumentsAndCorrespondingOptions) {
    // one arguments
    {
        auto args = generateGroupedArguments({"arg0"});
        CHECK_EQUAL(args.size(), 1);
        CHECK_EQUAL(args[0].argc(), 1);
        CHECK_EQUAL(std::string{args[0].argv()[0]}, std::string{"arg0"});
    }
    // one arguments with options
    {
        auto args = generateGroupedArguments({"arg0", "--option0", "--option1"});
        CHECK_EQUAL(args.size(), 1);
        CHECK_EQUAL(args[0].argc(), 3);
        CHECK_EQUAL(std::string{args[0].argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{args[0].argv()[1]}, std::string{"--option0"});
        CHECK_EQUAL(std::string{args[0].argv()[2]}, std::string{"--option1"});
    }
    // two arguments
    {
        auto args = generateGroupedArguments({"arg0", "arg1", "--option1"});
        CHECK_EQUAL(args.size(), 2);

        CHECK_EQUAL(args[0].argc(), 1);
        CHECK_EQUAL(std::string{args[0].argv()[0]}, std::string{"arg0"});
        
        CHECK_EQUAL(args[1].argc(), 2);
        CHECK_EQUAL(std::string{args[1].argv()[0]}, std::string{"arg1"});
        CHECK_EQUAL(std::string{args[1].argv()[1]}, std::string{"--option1"});
    }
}

SARUS_UNITTEST_MAIN_FUNCTION();
