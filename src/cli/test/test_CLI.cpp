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
    CHECK_THROWS(common::Error, generateCommandFromCLIArguments({"sarus", "---run"}));
}

std::shared_ptr<common::Config> generateConfig(const common::CLIArguments& args) {
    auto commandName = args.argv()[0];
    auto configRAII = test_utility::config::makeConfig();
    auto factory = cli::CommandObjectsFactory{};
    auto command = factory.makeCommandObject(commandName, args, configRAII.config);
    return configRAII.config;
}

TEST(CLITestGroup, generated_config_for_CommandLoad) {
    // centralized repo
    {
        auto conf = generateConfig(
            {"load", "--centralized-repository", "archive.tar", "library/image:tag"});
        auto expectedArchivePath = boost::filesystem::absolute("archive.tar");
        CHECK_EQUAL(conf->directories.tempFromCLI.empty(), true);
        CHECK_EQUAL(conf->useCentralizedRepository, true);
        CHECK_EQUAL(conf->archivePath.string(), expectedArchivePath.string());
        CHECK_EQUAL(conf->imageID.server, std::string{"load"});
        CHECK_EQUAL(conf->imageID.repositoryNamespace, std::string{"library"});
        CHECK_EQUAL(conf->imageID.image, std::string{"image"});
        CHECK_EQUAL(conf->imageID.tag, std::string{"tag"});
    }
    // temp dir
    {
        auto conf = generateConfig(
            {"load", "--temp-dir=/custom-temp-dir", "archive.tar", "library/image:tag"});
        auto expectedArchivePath = boost::filesystem::absolute("archive.tar");
        CHECK_EQUAL(conf->directories.temp.string(), std::string{"/custom-temp-dir"});
        CHECK_EQUAL(conf->useCentralizedRepository, false);
        CHECK_EQUAL(conf->archivePath.string(), expectedArchivePath.string());
        CHECK_EQUAL(conf->imageID.server, std::string{"load"});
        CHECK_EQUAL(conf->imageID.repositoryNamespace, std::string{"library"});
        CHECK_EQUAL(conf->imageID.image, std::string{"image"});
        CHECK_EQUAL(conf->imageID.tag, std::string{"tag"});
    }
}

TEST(CLITestGroup, generated_config_for_CommandPull) {
    // defaults
    {
        auto conf = generateConfig({"pull", "ubuntu"});
        CHECK(conf->directories.tempFromCLI.empty() == true);
        CHECK(conf->useCentralizedRepository == false);
        CHECK(conf->imageID.server == "index.docker.io");
        CHECK(conf->imageID.repositoryNamespace == "library");
        CHECK(conf->imageID.image == "ubuntu");
        CHECK(conf->imageID.tag == "latest");
    }
    // centralized repo
    {
        auto conf = generateConfig({"pull", "--centralized-repository", "ubuntu"});
        CHECK(conf->directories.tempFromCLI.empty() == true);
        CHECK(conf->useCentralizedRepository == true);
        CHECK(conf->imageID.server == "index.docker.io");
        CHECK(conf->imageID.repositoryNamespace == "library");
        CHECK(conf->imageID.image == "ubuntu");
        CHECK(conf->imageID.tag == "latest");
    }
    // temp-dir option and custom server
    {
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
}

TEST(CLITestGroup, generated_config_for_CommandRmi) {
    // defaults
    {
        auto conf = generateConfig({"rmi", "ubuntu"});
        CHECK_EQUAL(conf->useCentralizedRepository, false);
        CHECK_EQUAL(conf->imageID.server, std::string{"index.docker.io"});
        CHECK_EQUAL(conf->imageID.repositoryNamespace, std::string{"library"});
        CHECK_EQUAL(conf->imageID.image, std::string{"ubuntu"});
        CHECK_EQUAL(conf->imageID.tag, std::string{"latest"});
    }
    // centralized repo
    {
        auto conf = generateConfig({"rmi", "--centralized-repository", "ubuntu"});
        CHECK_EQUAL(conf->useCentralizedRepository, true);
        CHECK_EQUAL(conf->imageID.server, std::string{"index.docker.io"});
        CHECK_EQUAL(conf->imageID.repositoryNamespace, std::string{"library"});
        CHECK_EQUAL(conf->imageID.image, std::string{"ubuntu"});
        CHECK_EQUAL(conf->imageID.tag, std::string{"latest"});
    }
}

TEST(CLITestGroup, generated_config_for_CommandRun) {
    // empty values
    {
        auto conf = generateConfig({"run", "image"});
        CHECK_EQUAL(conf->imageID.server, std::string{"index.docker.io"});
        CHECK_EQUAL(conf->imageID.repositoryNamespace, std::string{"library"});
        CHECK_EQUAL(conf->imageID.image, std::string{"image"});
        CHECK_EQUAL(conf->imageID.tag, std::string{"latest"});
        CHECK_EQUAL(conf->useCentralizedRepository, false);
        CHECK_EQUAL(conf->commandRun.addInitProcess, false);
        CHECK_EQUAL(conf->commandRun.mounts.size(), 1); // 1 site mount + 0 user mount
        CHECK_EQUAL(conf->commandRun.useMPI, false);
        CHECK_EQUAL(conf->commandRun.enableAmdGpu, false);
        CHECK_EQUAL(conf->commandRun.enableGlibcReplacement, 0);
        CHECK_EQUAL(conf->commandRun.enableSSH, false);
        CHECK_EQUAL(conf->commandRun.allocatePseudoTTY, false);
        CHECK(conf->commandRun.execArgs.argc() == 0);
    }
    // centralized repository
    {
        auto conf = generateConfig({"run", "--centralized-repository", "image"});
        CHECK_EQUAL(conf->useCentralizedRepository, true);
    }
    // entrypoint
    {
        auto conf = generateConfig({"run", "--entrypoint", "myprogram", "image"});
        CHECK_EQUAL(conf->commandRun.entrypoint->argc(), 1);
        CHECK_EQUAL(conf->commandRun.entrypoint->argv()[0], std::string{"myprogram"});

        conf = generateConfig({"run", "--entrypoint", "myprogram --option", "image"});
        CHECK_EQUAL(conf->commandRun.entrypoint->argc(), 2);
        CHECK_EQUAL(conf->commandRun.entrypoint->argv()[0], std::string{"myprogram"});
        CHECK_EQUAL(conf->commandRun.entrypoint->argv()[1], std::string{"--option"});
    }
    // init
    {
        auto conf = generateConfig({"run", "--init", "image"});
        CHECK_EQUAL(conf->commandRun.addInitProcess, true);
    }
    // mount
    {
        auto conf = generateConfig({"run", "--mount",
                               "type=bind,source=/source,destination=/destination",
                               "image"});
        CHECK_EQUAL(conf->commandRun.mounts.size(), 2); // 1 site mount + 1 user mount
    }
    // mpi
    {
        auto conf = generateConfig({"run", "--mpi", "image"});
        CHECK_EQUAL(conf->commandRun.useMPI, true);

        conf = generateConfig({"run", "-m", "image"});
        CHECK_EQUAL(conf->commandRun.useMPI, true);
    }
    // amdgpu
    {
        auto conf = generateConfig({"run", "--amdgpu", "image"});
        CHECK_EQUAL(conf->commandRun.enableAmdGpu, true);
    }
    // ssh
    {
        auto conf = generateConfig({"run", "--ssh", "image"});
        CHECK_EQUAL(conf->commandRun.enableSSH, true);
    }
    // tty
    {
        auto conf = generateConfig({"run", "--tty", "image"});
        CHECK_EQUAL(conf->commandRun.allocatePseudoTTY, true);

        conf = generateConfig({"run", "-t", "image"});
        CHECK_EQUAL(conf->commandRun.allocatePseudoTTY, true);
    }
    // workdir
    {
        // long option with whitespace
        auto conf = generateConfig({"run", "--workdir", "/workdir", "image"});
        CHECK_EQUAL(conf->commandRun.workdir->string(), std::string{"/workdir"});
        // short option with whitespace
        conf = generateConfig({"run", "-w", "/workdir", "image"});
        CHECK_EQUAL(conf->commandRun.workdir->string(), std::string{"/workdir"});
    }
    // sticky short options
    {
        auto conf = generateConfig({"run", "-mt", "image"});
        CHECK_EQUAL(conf->commandRun.useMPI, true);
        CHECK_EQUAL(conf->commandRun.allocatePseudoTTY, true);
    }
    // options as application arguments (for images with an entrypoint)
    {
        auto conf = generateConfig({"run", "image", "--option0", "--option1", "-q"});
        CHECK(conf->commandRun.execArgs.argc() == 3);
        CHECK_EQUAL(conf->commandRun.execArgs.argv()[0], std::string{"--option0"});
        CHECK_EQUAL(conf->commandRun.execArgs.argv()[1], std::string{"--option1"});
        CHECK_EQUAL(conf->commandRun.execArgs.argv()[2], std::string{"-q"});
    }
    // combined test
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

SARUS_UNITTEST_MAIN_FUNCTION();
