/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <memory>
#include <string>

#include <boost/filesystem.hpp>
#include <rapidjson/document.h>
#include <rapidjson/schema.h>

#include "libsarus/Utility.hpp"
#include "common/Config.hpp"
#include "libsarus/CLIArguments.hpp"
#include "libsarus/Mount.hpp"
#include "cli/CommandObjectsFactory.hpp"
#include "cli/CLI.hpp"
#include "cli/CommandHelp.hpp"
#include "cli/CommandHelpOfCommand.hpp"
#include "cli/CommandHooks.hpp"
#include "cli/CommandImages.hpp"
#include "cli/CommandKill.hpp"
#include "cli/CommandLoad.hpp"
#include "cli/CommandPs.hpp"
#include "cli/CommandPull.hpp"
#include "cli/CommandRmi.hpp"
#include "cli/CommandRun.hpp"
#include "cli/CommandSshKeygen.hpp"
#include "cli/CommandVersion.hpp"
#include "test_utility/config.hpp"
#include "test_utility/filesystem.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace rj = rapidjson;
using namespace sarus;

TEST_GROUP(CLITestGroup) {
};

std::unique_ptr<cli::Command> generateCommandFromCLIArguments(const libsarus::CLIArguments& args) {
    auto cli = cli::CLI{};
    auto configRAII = test_utility::config::makeConfig();
    return cli.parseCommandLine(args, configRAII.config);
}

template<class ExpectedDynamicType>
void checkCommandDynamicType(const cli::Command& command) {
    CHECK(dynamic_cast<const ExpectedDynamicType*>(&command) != nullptr);
}

TEST(CLITestGroup, LogLevel) {
    auto& logger = libsarus::Logger::getInstance();
    generateCommandFromCLIArguments({"sarus"});
    CHECK(logger.getLevel() == libsarus::LogLevel::WARN);

    generateCommandFromCLIArguments({"sarus", "--verbose"});
    CHECK(logger.getLevel() == libsarus::LogLevel::INFO);

    generateCommandFromCLIArguments({"sarus", "--debug"});
    CHECK(logger.getLevel() == libsarus::LogLevel::DEBUG);
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

    command = generateCommandFromCLIArguments({"sarus", "hooks"});
    checkCommandDynamicType<cli::CommandHooks>(*command);

    command = generateCommandFromCLIArguments({"sarus", "images"});
    checkCommandDynamicType<cli::CommandImages>(*command);

    command = generateCommandFromCLIArguments({"sarus", "kill", "name"});
    checkCommandDynamicType<cli::CommandKill>(*command);

    command = generateCommandFromCLIArguments({"sarus", "load", "archive.tar", "image"});
    checkCommandDynamicType<cli::CommandLoad>(*command);

    command = generateCommandFromCLIArguments({"sarus", "ps"});
    checkCommandDynamicType<cli::CommandPs>(*command);

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
    CHECK_THROWS(libsarus::Error, generateCommandFromCLIArguments({"sarus", "--mpi", "run"}));
    CHECK_THROWS(libsarus::Error, generateCommandFromCLIArguments({"sarus", "---run"}));
}

std::shared_ptr<common::Config> generateConfig(const libsarus::CLIArguments& args) {
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
        CHECK_EQUAL(conf->imageReference.server, std::string{"load"});
        CHECK_EQUAL(conf->imageReference.repositoryNamespace, std::string{"library"});
        CHECK_EQUAL(conf->imageReference.image, std::string{"image"});
        CHECK_EQUAL(conf->imageReference.tag, std::string{"tag"});
    }
    // temp dir
    {
        auto customTempDir = libsarus::PathRAII("/tmp/sarus-utest-temp-dir");
        libsarus::filesystem::createFoldersIfNecessary(customTempDir.getPath());

        auto conf = generateConfig(
            {"load", "--temp-dir="+customTempDir.getPath().string(), "archive.tar", "library/image:tag"});
        auto expectedArchivePath = boost::filesystem::absolute("archive.tar");
        CHECK_EQUAL(conf->directories.temp.string(), customTempDir.getPath().string());
        CHECK_EQUAL(conf->useCentralizedRepository, false);
        CHECK_EQUAL(conf->archivePath.string(), expectedArchivePath.string());
        CHECK_EQUAL(conf->imageReference.server, std::string{"load"});
        CHECK_EQUAL(conf->imageReference.repositoryNamespace, std::string{"library"});
        CHECK_EQUAL(conf->imageReference.image, std::string{"image"});
        CHECK_EQUAL(conf->imageReference.tag, std::string{"tag"});
    }
}

TEST(CLITestGroup, generated_config_for_CommandPull) {
    // defaults
    {
        auto conf = generateConfig({"pull", "ubuntu"});
        CHECK(conf->directories.tempFromCLI.empty() == true);
        CHECK(conf->useCentralizedRepository == false);
        CHECK(conf->authentication.isAuthenticationNeeded == false);
        CHECK(conf->authentication.username.empty());
        CHECK(conf->authentication.password.empty());
        CHECK(conf->imageReference.server == "docker.io");
        CHECK(conf->imageReference.repositoryNamespace == "library");
        CHECK(conf->imageReference.image == "ubuntu");
        CHECK(conf->imageReference.tag == "latest");
    }
    // centralized repo
    {
        auto conf = generateConfig({"pull", "--centralized-repository", "ubuntu"});
        CHECK(conf->directories.tempFromCLI.empty() == true);
        CHECK(conf->useCentralizedRepository == true);
        CHECK(conf->authentication.isAuthenticationNeeded == false);
        CHECK(conf->authentication.username.empty());
        CHECK(conf->authentication.password.empty());
        CHECK(conf->imageReference.server == "docker.io");
        CHECK(conf->imageReference.repositoryNamespace == "library");
        CHECK(conf->imageReference.image == "ubuntu");
        CHECK(conf->imageReference.tag == "latest");
    }
    // temp-dir option and custom server
    {
        auto customTempDir = libsarus::PathRAII("/tmp/sarus-utest-temp-dir");
        libsarus::filesystem::createFoldersIfNecessary(customTempDir.getPath());

        auto conf = generateConfig(
            {"pull",
            "--temp-dir="+customTempDir.getPath().string(),
            "my.own.server:5000/user/image:tag"});
        CHECK(conf->directories.temp.string() == customTempDir.getPath().string());
        CHECK(conf->authentication.isAuthenticationNeeded == false);
        CHECK(conf->authentication.username.empty());
        CHECK(conf->authentication.password.empty());
        CHECK(conf->imageReference.server == "my.own.server:5000");
        CHECK(conf->imageReference.repositoryNamespace == "user");
        CHECK(conf->imageReference.image == "image");
        CHECK(conf->imageReference.tag == "tag");
    }
    // username
    {
        auto conf = generateConfig({"pull", "--username", "alice", "ubuntu"});
        CHECK(conf->authentication.isAuthenticationNeeded == true);
        CHECK(conf->authentication.username == "alice");

        conf = generateConfig({"pull", "-u", "bob", "ubuntu"});
        CHECK(conf->authentication.isAuthenticationNeeded == true);
        CHECK(conf->authentication.username == "bob");
    }
}

TEST(CLITestGroup, generated_config_for_CommandRmi) {
    // defaults
    {
        auto conf = generateConfig({"rmi", "ubuntu"});
        CHECK_EQUAL(conf->useCentralizedRepository, false);
        CHECK_EQUAL(conf->imageReference.server, std::string{"docker.io"});
        CHECK_EQUAL(conf->imageReference.repositoryNamespace, std::string{"library"});
        CHECK_EQUAL(conf->imageReference.image, std::string{"ubuntu"});
        CHECK_EQUAL(conf->imageReference.tag, std::string{"latest"});
    }
    // centralized repo
    {
        auto conf = generateConfig({"rmi", "--centralized-repository", "ubuntu"});
        CHECK_EQUAL(conf->useCentralizedRepository, true);
        CHECK_EQUAL(conf->imageReference.server, std::string{"docker.io"});
        CHECK_EQUAL(conf->imageReference.repositoryNamespace, std::string{"library"});
        CHECK_EQUAL(conf->imageReference.image, std::string{"ubuntu"});
        CHECK_EQUAL(conf->imageReference.tag, std::string{"latest"});
    }
}

TEST(CLITestGroup, generated_config_for_CommandRun) {
    // empty values
    {
        auto conf = generateConfig({"run", "image"});
        CHECK_EQUAL(conf->imageReference.server, std::string{"docker.io"});
        CHECK_EQUAL(conf->imageReference.repositoryNamespace, std::string{"library"});
        CHECK_EQUAL(conf->imageReference.image, std::string{"image"});
        CHECK_EQUAL(conf->imageReference.tag, std::string{"latest"});

        CHECK_FALSE(conf->useCentralizedRepository);

        CHECK_EQUAL(conf->commandRun.userEnvironment.size(), 0);
        CHECK_EQUAL(conf->commandRun.mounts.size(), 1); // 1 site mount + 0 user mount
        CHECK_EQUAL(conf->commandRun.ociAnnotations.size(), 1); // 1 annotation already in test config
        CHECK_FALSE(conf->commandRun.createNewPIDNamespace);
        CHECK_FALSE(conf->commandRun.addInitProcess);
        CHECK_FALSE(conf->commandRun.useMPI);
        CHECK_FALSE(conf->commandRun.enableGlibcReplacement);
        CHECK_FALSE(conf->commandRun.enableSSH);
        CHECK_FALSE(conf->commandRun.allocatePseudoTTY);
        CHECK_FALSE(conf->commandRun.mpiType);
        CHECK(conf->commandRun.execArgs.argc() == 0);
    }
    //annotation
    {
        auto conf = generateConfig({"run", "--annotation=key=value", "image"});
        CHECK_EQUAL(conf->commandRun.ociAnnotations.size(), 2);
        CHECK_EQUAL(conf->commandRun.ociAnnotations["com.test.dummy_key"], std::string("dummy_value"));
        CHECK_EQUAL(conf->commandRun.ociAnnotations["key"], std::string("value"));

        conf = generateConfig({"run",
                               "--annotation", "normal.annotation.key=value",
                               "--annotation=nested.annotation.key=innerKey=innerValue",
                               "--annotation", "empty_annotation=",
                               "--annotation=no_separator",
                               "image"});
        CHECK_EQUAL(conf->commandRun.ociAnnotations.size(), 5);
        CHECK_EQUAL(conf->commandRun.ociAnnotations["normal.annotation.key"], std::string("value"));
        CHECK_EQUAL(conf->commandRun.ociAnnotations["nested.annotation.key"], std::string("innerKey=innerValue"));
        CHECK_EQUAL(conf->commandRun.ociAnnotations["empty_annotation"], std::string(""));
        CHECK_EQUAL(conf->commandRun.ociAnnotations["no_separator"], std::string(""));

        conf = generateConfig({"run", "--annotation=com.test.dummy_key=overridden_value", "image"});
        CHECK_EQUAL(conf->commandRun.ociAnnotations.size(), 1);
        CHECK_EQUAL(conf->commandRun.ociAnnotations["com.test.dummy_key"], std::string("overridden_value"));
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
    // env
    {
        auto conf = generateConfig({"run", "--env=NAME=value", "image"});
        CHECK_EQUAL(conf->commandRun.userEnvironment.size(), 1);
        CHECK_EQUAL(conf->commandRun.userEnvironment["NAME"], std::string{"value"});

        conf = generateConfig({"run",
                               "-e", "NAME=value", "--env", "NESTED=innerKey=innerValue",
                               "-e", "CONTAINER=sarus",
                               "image"});
        CHECK_EQUAL(conf->commandRun.userEnvironment.size(), 3);
        CHECK_EQUAL(conf->commandRun.userEnvironment["NAME"], std::string{"value"});
        CHECK_EQUAL(conf->commandRun.userEnvironment["NESTED"], std::string{"innerKey=innerValue"});
        CHECK_EQUAL(conf->commandRun.userEnvironment["CONTAINER"], std::string{"sarus"});

        conf = generateConfig({"run", "--env=EMPTY=", "image"});
        CHECK_EQUAL(conf->commandRun.userEnvironment.size(), 1);
        CHECK_EQUAL(conf->commandRun.userEnvironment["EMPTY"], std::string{""});

        conf = generateConfig({"run", "--env=key", "image"});
        CHECK_EQUAL(conf->commandRun.userEnvironment.size(), 1);
        CHECK_EQUAL(conf->commandRun.userEnvironment["key"], std::string{"value"});

        conf = generateConfig({"run", "--env=INEXISTENT", "image"});
        CHECK_TRUE(conf->commandRun.userEnvironment.empty());
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
        CHECK_FALSE(conf->commandRun.mpiType);

        conf = generateConfig({"run", "-m", "image"});
        CHECK_EQUAL(conf->commandRun.useMPI, true);
        CHECK_FALSE(conf->commandRun.mpiType);

        // default MPI type from JSON configuration
        auto configRAII = test_utility::config::makeConfig();
        conf = configRAII.config;
        auto& allocator = conf->json.GetAllocator();
        conf->json.AddMember("defaultMPIType",
                             rj::Value{"testDefaultMPI"},
                             allocator);
        auto factory = cli::CommandObjectsFactory{};
        factory.makeCommandObject("run", {"run", "--mpi", "image"}, conf);
        CHECK_EQUAL(conf->commandRun.useMPI, true);
        CHECK_EQUAL(*conf->commandRun.mpiType, std::string{"testDefaultMPI"});
    }
    // mpi-type
    {
        auto conf = generateConfig({"run", "--mpi-type", "mpi0", "image"});
        CHECK_EQUAL(conf->commandRun.useMPI, true);
        CHECK_EQUAL(*conf->commandRun.mpiType, std::string{"mpi0"});

        conf = generateConfig({"run", "--mpi", "--mpi-type", "mpi1", "image"});
        CHECK_EQUAL(conf->commandRun.useMPI, true);
        CHECK_EQUAL(*conf->commandRun.mpiType, std::string{"mpi1"});
    }
    // pid
    {
        auto conf = generateConfig({"run", "--pid", "host", "image"});
        CHECK_EQUAL(conf->commandRun.createNewPIDNamespace, false);

        conf = generateConfig({"run", "--pid", "private", "image"});
        CHECK_EQUAL(conf->commandRun.createNewPIDNamespace, true);
    }
    // ssh
    {
        auto conf = generateConfig({"run", "--ssh", "image"});
        CHECK_EQUAL(conf->commandRun.enableSSH, true);
        CHECK_EQUAL(conf->commandRun.createNewPIDNamespace, true);
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
    // name
    {
        auto conf = generateConfig({"run", "image"});
        CHECK_FALSE(conf->commandRun.containerName);
        conf = generateConfig({"run", "--name", "test", "image"});
        CHECK_EQUAL(conf->commandRun.containerName.get(),  std::string{"test"});
        conf = generateConfig({"run", "-n", "test", "image"});
        CHECK_EQUAL(conf->commandRun.containerName.get(),  std::string{"test"});
    }
    // combined test
    {
        auto conf = generateConfig({"run",
                                    "-e", "CONTAINER=sarus",
                                    "--workdir=/workdir",
                                    "--mpi",
                                    "--glibc",
                                    "--mount=type=bind,source=/source,destination=/destination",
                                    "ubuntu", "bash", "-c", "ls /dev |grep nvidia"});
        CHECK_EQUAL(conf->commandRun.workdir->string(), std::string{"/workdir"});
        CHECK_EQUAL(conf->commandRun.createNewPIDNamespace, false);
        CHECK_EQUAL(conf->commandRun.useMPI, true);
        CHECK_EQUAL(conf->commandRun.enableGlibcReplacement, true);
        CHECK_EQUAL(conf->commandRun.enableSSH, false);
        CHECK_EQUAL(conf->commandRun.userEnvironment.size(), 1);
        CHECK_EQUAL(conf->commandRun.userEnvironment["CONTAINER"], std::string{"sarus"});
        CHECK_EQUAL(conf->commandRun.mounts.size(), 2); // 1 site mount + 1 user mount
        CHECK_EQUAL(conf->imageReference.server, std::string{"docker.io"});
        CHECK_EQUAL(conf->imageReference.repositoryNamespace, std::string{"library"});
        CHECK_EQUAL(conf->imageReference.image, std::string{"ubuntu"});
        CHECK_EQUAL(conf->imageReference.tag, std::string{"latest"});
        CHECK(conf->commandRun.execArgs.argc() == 3);
        CHECK_EQUAL(conf->commandRun.execArgs.argv()[0], std::string{"bash"});
        CHECK_EQUAL(conf->commandRun.execArgs.argv()[1], std::string{"-c"});
        CHECK_EQUAL(conf->commandRun.execArgs.argv()[2], std::string{"ls /dev |grep nvidia"});
    }
}

std::shared_ptr<common::Config> generateConfigWithSiteDevice(const libsarus::CLIArguments& args,
                                                             const boost::filesystem::path& devicePath,
                                                             const boost::filesystem::path& mountDestination,
                                                             const std::string& deviceAccess) {
    auto commandName = args.argv()[0];
    auto configRAII = test_utility::config::makeConfig();

    auto& allocator = configRAII.config->json.GetAllocator();
    rj::Value siteDevicesValue(rj::kArrayType);
    rj::Value deviceValue(rj::kObjectType);
    deviceValue.AddMember("source",
                          rj::Value{devicePath.c_str(), allocator},
                          allocator);
    if (!mountDestination.empty()) {
    deviceValue.AddMember("destination",
                          rj::Value{mountDestination.c_str(), allocator},
                          allocator);
    }
    if (!deviceAccess.empty()) {
    deviceValue.AddMember("access",
                          rj::Value{deviceAccess.c_str(), allocator},
                          allocator);
    }
    siteDevicesValue.PushBack(deviceValue, allocator);
    configRAII.config->json.AddMember("siteDevices", siteDevicesValue, allocator);

    auto factory = cli::CommandObjectsFactory{};
    auto command = factory.makeCommandObject(commandName, args, configRAII.config);
    return configRAII.config;
}

#ifdef ASROOT
TEST (CLITestGroup, device_mounts_for_CommandRun)  {
#else
IGNORE_TEST(CLITestGroup, device_mounts_for_CommandRun) {
#endif
    auto testDir = libsarus::PathRAII(
        libsarus::filesystem::makeUniquePathWithRandomSuffix(boost::filesystem::current_path() / "CLI-test-deviceMounts-CommandRun"));
    libsarus::filesystem::createFoldersIfNecessary(testDir.getPath());
    auto siteDevice = testDir.getPath() / "siteDevice";
    auto siteDeviceMajorID = 511u;
    auto siteDeviceMinorID = 511u;
    test_utility::filesystem::createCharacterDeviceFile(siteDevice, siteDeviceMajorID, siteDeviceMinorID);

    auto userDevice0 = testDir.getPath() / "userDevice0";
    auto userDevice0MajorID = 500u; auto userDevice0MinorID = 500u;
    test_utility::filesystem::createCharacterDeviceFile(userDevice0, userDevice0MajorID, userDevice0MinorID);

    auto userDevice1 = testDir.getPath() / "userDevice1";
    auto userDevice1MajorID = 501u; auto userDevice1MinorID = 501u;
    test_utility::filesystem::createCharacterDeviceFile(userDevice1, userDevice1MajorID, userDevice1MinorID);

    // only siteDevices, implicit mount destination
    {
        auto siteDestination = boost::filesystem::path("");
        auto siteAcess = std::string{""};
        auto conf = generateConfigWithSiteDevice({"run", "image"}, siteDevice, siteDestination, siteAcess);
        CHECK_EQUAL(conf->commandRun.deviceMounts.size(), 1); // 1 site device + 0 user device
        CHECK(conf->commandRun.deviceMounts.front()->getSource() == siteDevice);
        CHECK(conf->commandRun.deviceMounts.front()->getDestination() == siteDevice);
        CHECK(conf->commandRun.deviceMounts.front()->getAccess().string() == std::string("rwm"));
    }
    // only siteDevices, explicit mount destination
    {
        auto siteDestination = boost::filesystem::path("/dev/siteDevice");
        auto siteAcess = std::string{""};
        auto conf = generateConfigWithSiteDevice({"run", "image"}, siteDevice, siteDestination, siteAcess);
        CHECK_EQUAL(conf->commandRun.deviceMounts.size(), 1); // 1 site device + 0 user device
        CHECK(conf->commandRun.deviceMounts.front()->getSource() == siteDevice);
        CHECK(conf->commandRun.deviceMounts.front()->getDestination() == siteDestination);
        CHECK(conf->commandRun.deviceMounts.front()->getAccess().string() == std::string("rwm"));
    }
    // only siteDevices, non-default access
    {
        auto siteDestination = boost::filesystem::path("/dev/siteDevice");
        auto siteAcess = std::string{"rw"};
        auto conf = generateConfigWithSiteDevice({"run", "image"}, siteDevice, siteDestination, siteAcess);
        CHECK_EQUAL(conf->commandRun.deviceMounts.size(), 1); // 1 site device + 0 user device
        CHECK(conf->commandRun.deviceMounts.front()->getSource() == siteDevice);
        CHECK(conf->commandRun.deviceMounts.front()->getDestination() == siteDestination);
        CHECK(conf->commandRun.deviceMounts.front()->getAccess().string() == std::string("rw"));
    }
    // only siteDevices, explicit mount destination and non-default access
    {
        auto siteDestination = boost::filesystem::path("/dev/siteDevice");
        auto siteAcess = std::string{"r"};
        auto conf = generateConfigWithSiteDevice({"run", "image"}, siteDevice, siteDestination, siteAcess);
        CHECK_EQUAL(conf->commandRun.deviceMounts.size(), 1); // 1 site device + 0 user device
        CHECK(conf->commandRun.deviceMounts.front()->getSource() == siteDevice);
        CHECK(conf->commandRun.deviceMounts.front()->getDestination() == siteDestination);
        CHECK(conf->commandRun.deviceMounts.front()->getAccess().string() == std::string("r"));
    }
    // only CLI option
    {
        auto option0Value = userDevice0.string() + ":/dev/userDevice0:rw";
        auto option1 = "--device=" + userDevice1.string() + ":r";
        auto conf = generateConfig({"run", "--device", option0Value, option1, "image"});
        CHECK_EQUAL(conf->commandRun.deviceMounts.size(), 2);
        CHECK(conf->commandRun.deviceMounts[0]->getSource() == userDevice0);
        CHECK(conf->commandRun.deviceMounts[0]->getDestination() == boost::filesystem::path("/dev/userDevice0"));
        CHECK(conf->commandRun.deviceMounts[0]->getAccess().string() == std::string("rw"));
        CHECK(conf->commandRun.deviceMounts[1]->getSource() == userDevice1);
        CHECK(conf->commandRun.deviceMounts[1]->getDestination() == boost::filesystem::path(userDevice1));
        CHECK(conf->commandRun.deviceMounts[1]->getAccess().string() == std::string("r"));
    }
    // combine siteDevices and CLI option
    {
        auto siteDestination = boost::filesystem::path("");
        auto siteAcess = std::string{"rw"};
        auto conf = generateConfigWithSiteDevice({"run", "--device", userDevice0.string(), "image"}, siteDevice, siteDestination, siteAcess);
        CHECK_EQUAL(conf->commandRun.deviceMounts.size(), 2);
        CHECK(conf->commandRun.deviceMounts[0]->getSource() == siteDevice);
        CHECK(conf->commandRun.deviceMounts[0]->getDestination() == siteDevice);
        CHECK(conf->commandRun.deviceMounts[0]->getAccess().string() == std::string("rw"));
        CHECK(conf->commandRun.deviceMounts[1]->getSource() == userDevice0);
        CHECK(conf->commandRun.deviceMounts[1]->getDestination() == userDevice0);
        CHECK(conf->commandRun.deviceMounts[1]->getAccess().string() == std::string("rwm"));
        // prefer site destination and access for the same device
        siteDestination = boost::filesystem::path("/dev/siteDevice");
        conf = generateConfigWithSiteDevice({"run", "--device", siteDevice.string(), "image"}, siteDevice, siteDestination, siteAcess);
        CHECK_EQUAL(conf->commandRun.deviceMounts.size(), 1);
        CHECK(conf->commandRun.deviceMounts[0]->getSource() == siteDevice);
        CHECK(conf->commandRun.deviceMounts[0]->getDestination() == siteDestination);
        CHECK(conf->commandRun.deviceMounts[0]->getAccess().string() == std::string("rw"));
    }
}

SARUS_UNITTEST_MAIN_FUNCTION();
