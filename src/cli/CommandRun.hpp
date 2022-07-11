/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_CommandRun_hpp
#define cli_CommandRun_hpp

#include <iostream>
#include <stdexcept>
#include <chrono>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include "common/Utility.hpp"
#include "common/Config.hpp"
#include "common/CLIArguments.hpp"
#include "cli/Utility.hpp"
#include "cli/Command.hpp"
#include "cli/MountParser.hpp"
#include "cli/DeviceParser.hpp"
#include "cli/HelpMessage.hpp"
#include "image_manager/ImageStore.hpp"
#include "runtime/Runtime.hpp"
#include "runtime/DeviceMount.hpp"


namespace sarus {
namespace cli {

class CommandRun : public Command {
public:
    CommandRun() {
        initializeOptionsDescription();
    }

    CommandRun(const common::CLIArguments& args, std::shared_ptr<common::Config> conf)
        : conf{std::move(conf)}
    {
        initializeOptionsDescription();
        parseCommandArguments(args);
    }

    void execute() override {
        cli::utility::printLog("Executing run command", common::LogLevel::INFO);

        if(conf->commandRun.enableSSH && !checkUserHasSshKeys()) {
            auto message = boost::format("Failed to check the SSH keys. Hint: try to"
                                         " generate the SSH keys with 'sarus ssh-keygen'.");
            common::Logger::getInstance().log(message, "CLI", common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }

        verifyThatImageIsAvailable();

        auto setupBegin = std::chrono::high_resolution_clock::now();
        auto cliTime = std::chrono::duration<double>(setupBegin - conf->program_start);
        auto message = boost::format("Processed CLI arguments in %.6f seconds") % cliTime.count();
        cli::utility::printLog(message, common::LogLevel::INFO);

        auto runtime = runtime::Runtime{conf};
        runtime.setupOCIBundle();

        auto setupEnd = std::chrono::high_resolution_clock::now();
        auto setupTime = std::chrono::duration<double>(setupEnd - setupBegin);
        message = boost::format("Successfully set up container in %.6f seconds") % setupTime.count();
        cli::utility::printLog(message, common::LogLevel::INFO);

        runtime.executeContainer();

        cli::utility::printLog("Successfully executed run command", common::LogLevel::INFO);
    }

    bool requiresRootPrivileges() const override {
        return true;
    }

    std::string getBriefDescription() const override {
        return  "Run a command in a new container";
    }

    void printHelpMessage() const override {
        auto printer = cli::HelpMessage()
            .setUsage("sarus run [OPTIONS] REPOSITORY[:TAG] [COMMAND] [ARG...]\n"
                "\n"
                "Note: REPOSITORY[:TAG] has to be specified as\n"
                "      displayed by the \"sarus images\" command.")
            .setDescription(getBriefDescription())
            .setOptionsDescription(optionsDescription);
        std::cout << printer;
    }

private:
    void initializeOptionsDescription() {
        optionsDescription.add_options()
            ("centralized-repository", "Use centralized repository instead of the local one")
            ("device",
                boost::program_options::value<std::vector<std::string>>(&deviceMounts),
                "Mount custom devices into the container")
            ("entrypoint",
                boost::program_options::value<std::string>(&entrypoint),
                "Overwrite the default ENTRYPOINT of the image")
            ("env,e",
                boost::program_options::value<std::vector<std::string>>(&env),
                "Set environment variables in the container")
            ("glibc", "Enable replacement of the container's GNU C libraries")
            ("init", "Run an init process inside the container that forwards signals and reaps processes. "
                     "Mostly useful in conjunction with '--pid=private'")
            ("mount",
                boost::program_options::value<std::vector<std::string>>(&conf->commandRun.userMounts),
                "Mount custom files and directories into the container")
            ("mpi,m", "Enable MPI support. Implies '--glibc'")
            ("pid",
                boost::program_options::value<std::string>(&pid),
                "Set the PID namespace mode for the container. Supported values: 'host', 'private'. "
                "Default: use the host’s PID namespace for the container")
            ("ssh", "Enable SSH in the container. Implies '--pid=private'")
            ("tty,t", "Allocate a pseudo-TTY in the container")
            ("workdir,w",
                boost::program_options::value<std::string>(&workdir),
                "Set working directory inside the container");
    }

    void parseCommandArguments(const common::CLIArguments& args) {
        cli::utility::printLog("parsing CLI arguments of run command", common::LogLevel::DEBUG);

        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = cli::utility::groupOptionsAndPositionalArguments(args, optionsDescription);

        // the run command expects at least one positional argument (the image name)
        cli::utility::validateNumberOfPositionalArguments(positionalArgs, 1, INT_MAX, "run");

        try {
            // Parse options
            boost::program_options::variables_map values;
            boost::program_options::parsed_options parsed =
                boost::program_options::command_line_parser(nameAndOptionArgs.argc(), nameAndOptionArgs.argv())
                        .options(optionsDescription)
                        .style(boost::program_options::command_line_style::unix_style)
                        .run();
            boost::program_options::store(parsed, values);
            boost::program_options::notify(values);

            // Parse the image reference and normalize it for consistency with Docker, Podman, Buildah
            conf->imageReference = cli::utility::parseImageReference(positionalArgs.argv()[0]).normalize();
            conf->useCentralizedRepository = values.count("centralized-repository");
            conf->directories.initialize(conf->useCentralizedRepository, *conf);
            // the remaining arguments (after image) are all part of the command to be executed in the container
            conf->commandRun.execArgs = common::CLIArguments(positionalArgs.begin()+1, positionalArgs.end());

            if(values.count("entrypoint")) {
                if(entrypoint.empty()) {
                    conf->commandRun.entrypoint = common::CLIArguments{};
                }
                else {
                    std::vector<std::string> entrypointArgs;
                    boost::algorithm::split(entrypointArgs, entrypoint, boost::algorithm::is_space());
                    conf->commandRun.entrypoint = common::CLIArguments{entrypointArgs.cbegin(), entrypointArgs.cend()};
                }
            }

            if(values.count("glibc")) {
                conf->commandRun.enableGlibcReplacement = true;
            }
            else {
                conf->commandRun.enableGlibcReplacement = false;
            }

            if(values.count("init")) {
                conf->commandRun.addInitProcess = true;
            }
            else {
                conf->commandRun.addInitProcess = false;
            }

            if(values.count("mpi")) {
                conf->commandRun.useMPI = true;
            }
            else {
                conf->commandRun.useMPI = false;
            }

            if(values.count("pid")) {
                if(pid == std::string{"private"}) {
                    conf->commandRun.createNewPIDNamespace = true;
                }
                else if(pid != std::string{"host"})  {
                    auto message = boost::format("Incorrect value provided for --pid option: '%s'. "
                                                 "Supported values: 'host', 'private'.") % pid;
                    SARUS_THROW_ERROR(message.str());
                }
            }
            else {
                conf->commandRun.createNewPIDNamespace = false;
            }

            if(values.count("ssh")) {
                conf->commandRun.enableSSH = true;
                conf->commandRun.createNewPIDNamespace = true;
                if(pid == std::string{"host"}) {
                    auto message = boost::format("The use of '--ssh' is incompatible with '--pid=host'. "
                                                 "The SSH hook requires the use of a private PID namespace");
                    SARUS_THROW_ERROR(message.str());
                }
            }
            else {
                conf->commandRun.enableSSH = false;
            }

            if(values.count("tty")) {
                conf->commandRun.allocatePseudoTTY = true;
            }
            else {
                conf->commandRun.allocatePseudoTTY = false;
            }

            if(values.count("workdir")) {
                conf->commandRun.workdir = workdir;
                if(!conf->commandRun.workdir->is_absolute()) {
                    auto message = boost::format("The working directory '%s' is invalid, it"
                                                 " needs to be an absolute path.") % workdir;
                    SARUS_THROW_ERROR(message.str());
                }
            }
        }
        catch (std::exception& e) {
            auto message = boost::format("%s\nSee 'sarus help run'") % e.what();
            cli::utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }

        makeUserEnvironment();
        makeSiteMountObjects();
        makeUserMountObjects();
        makeSiteDeviceMountObjects();
        makeUserDeviceMountObjects();

        cli::utility::printLog("successfully parsed CLI arguments", common::LogLevel::DEBUG);
    }

    void makeUserEnvironment() {
        for(const auto& variable : env) {
            auto message = boost::format("Parsing environment variable requested from CLI '%s'") % variable;
            cli::utility::printLog(message, common::LogLevel::DEBUG);

            if(variable.empty()) {
                auto message = boost::format("Invalid environment variable requested from CLI: empty option value");
                utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
                SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
            }

            std::string name, value;
            try {
                std::tie(name, value) = common::parseEnvironmentVariable(variable);
            } catch(common::Error& e) {
                auto message = boost::format("Environment variable requested from CLI '%s' does not feature '=' separator. "
                                             "Treating string as variable name and attempting to source value from host "
                                             "environment") % variable;
                cli::utility::printLog(message, common::LogLevel::INFO);

                auto& hostEnvironment = conf->commandRun.hostEnvironment;
                auto hostVariable = hostEnvironment.find(variable);
                if (hostVariable != hostEnvironment.cend()) {
                    name = hostVariable->first;
                    value = hostVariable->second;
                }
                else {
                    auto message = boost::format("Environment variable requested from CLI '%s' does not correspond to a variable "
                                                 "present in the host environment. Skipping request.") % variable;
                    cli::utility::printLog(message, common::LogLevel::INFO);
                    continue;
                }
            } catch(std::exception& e) {
                auto message = boost::format("Error parsing environment variable requested from CLI '%s': %s")
                               % variable % e.what();
                cli::utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
                SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
            }

            conf->commandRun.userEnvironment[name] = value;
            message = boost::format("Successfully parsed environment variable from CLI: Name: '%s' - Value: '%s'")
                      % name % value;
            cli::utility::printLog(message, common::LogLevel::DEBUG);
        }
    }

    void makeSiteMountObjects() {
        bool isUserMount = false;
        auto parser = cli::MountParser{isUserMount, conf};
        for(const auto& map : convertJSONSiteMountsToMaps()) {
            conf->commandRun.mounts.push_back(parser.parseMountRequest(map));
        }
    }

    void makeUserMountObjects() {
        bool isUserMount = true;
        auto parser = cli::MountParser{isUserMount, conf};
        for (const auto& mountString : conf->commandRun.userMounts) {
            auto map = common::parseMap(mountString);
            conf->commandRun.mounts.push_back(parser.parseMountRequest(map));
        }
    }

    std::vector<std::unordered_map<std::string, std::string>> convertJSONSiteMountsToMaps() {
        auto maps = std::vector<std::unordered_map<std::string, std::string>>{};

        try {
            if(!conf->json.HasMember("siteMounts")) {
                return maps;
            }

            for(const auto& mount : conf->json["siteMounts"].GetArray()) {
                auto map = std::unordered_map<std::string, std::string>{};
                for(const auto& field : mount.GetObject()) {
                    if(field.name.GetString() == std::string{"flags"}) {
                        for(const auto& flag : field.value.GetObject()) {
                            map[flag.name.GetString()] = flag.value.GetString();
                        }
                    }
                    else {
                        map[field.name.GetString()] = field.value.GetString();
                    }
                }
                maps.push_back(std::move(map));
            }
        } catch(const std::exception& e) {
            SARUS_RETHROW_ERROR(e, "Failed to convert JSON mount entry to map");
        }

        return maps;
    }

    void makeSiteDeviceMountObjects() {
        auto parser = cli::DeviceParser{conf};
        for (const auto& requestString : convertJSONSiteDevicesToStrings()) {
            try {
                conf->commandRun.deviceMounts.push_back(parser.parseDeviceRequest(requestString));
            }
            catch (const common::Error& e) {
                auto message = boost::format("Error while processing the 'siteDevices' parameter in the configuration file. "
                    "Please contact your system administrator");
                cli::utility::printLog(message, common::LogLevel::GENERAL, std::cerr);
                SARUS_RETHROW_ERROR(e, message.str(), common::LogLevel::INFO);
            }
        }
    }

    void makeUserDeviceMountObjects() {
        auto parser = cli::DeviceParser{conf};
        auto siteDevices = conf->commandRun.deviceMounts;
        for (const auto& requestString : deviceMounts) {
            auto deviceMount = std::move(parser.parseDeviceRequest(requestString));
            auto previousSiteDevice = findMatchingSiteDevice(deviceMount, siteDevices);
            if (previousSiteDevice) {
                auto message = boost::format("Device %s already added by the system administrator at container path %s with access %s. "
                                             "Skipping request from the command line")
                    % deviceMount->source % previousSiteDevice->destination % previousSiteDevice->getAccess().string();
                utility::printLog(message, common::LogLevel::WARN, std::cerr);
                continue;
            }
            conf->commandRun.deviceMounts.push_back(std::move(deviceMount));
        }
    }

    std::vector<std::string> convertJSONSiteDevicesToStrings() {
        auto vector = std::vector<std::string>{};

        try {
            if(!conf->json.HasMember("siteDevices")) {
                return vector;
            }

            for(const auto& device : conf->json["siteDevices"].GetArray()) {
                auto source = std::string{};
                auto destination = std::string{};
                auto access = std::string{};
                for(const auto& field : device.GetObject()) {
                    if(field.name.GetString() == std::string{"source"}) {
                        source = field.value.GetString();
                    }
                    else if (field.name.GetString() == std::string{"destination"}) {
                        destination = std::string{":"} + field.value.GetString();
                    }
                    else if (field.name.GetString() == std::string{"access"}) {
                        access = std::string{":"} + field.value.GetString();
                    }
                }
                vector.emplace_back(source + destination + access);
            }
        }
        catch(const std::exception& e) {
            SARUS_RETHROW_ERROR(e, "Failed to convert JSON device entry to string");
        }

        return vector;
    }

    std::shared_ptr<runtime::DeviceMount> findMatchingSiteDevice(const std::unique_ptr<runtime::DeviceMount>& deviceMount,
                                                                 const std::vector<std::shared_ptr<runtime::DeviceMount>> siteDevices) const {
        for (const auto& siteDevice : siteDevices) {
            if (deviceMount->source == siteDevice->source) {
                return siteDevice;
            }
        }
        return std::shared_ptr<runtime::DeviceMount>{};
    }

    bool checkUserHasSshKeys() const {
        cli::utility::printLog( "Checking that the user has SSH keys",
                                common::LogLevel::INFO);

        common::setEnvironmentVariable("HOOK_BASE_DIR=" + std::string{conf->json["localRepositoryBaseDir"].GetString()});

        auto passwdFile = boost::filesystem::path{ conf->json["prefixDir"].GetString() } / "etc/passwd";
        common::setEnvironmentVariable("PASSWD_FILE=" + passwdFile.string());

        auto args = common::CLIArguments{
            std::string{ conf->json["prefixDir"].GetString() } + "/bin/ssh_hook",
            "check-user-has-sshkeys"
        };
        if(sarus::common::Logger::getInstance().getLevel() == sarus::common::LogLevel::INFO) {
            args.push_back("--verbose");
        }
        else if(sarus::common::Logger::getInstance().getLevel() == sarus::common::LogLevel::DEBUG) {
            args.push_back("--debug");
        }

        auto setUserIdentity = [this]() {
            auto gid = conf->userIdentity.gid;
            if(setresgid(gid, gid, gid) != 0) {
                auto message = boost::format("Failed to setresgid(%1%, %1%, %1%): %2%") % gid % strerror(errno);
                SARUS_THROW_ERROR(message.str());
            }

            auto uid = conf->userIdentity.uid;
            if(setresuid(uid, uid, uid) != 0) {
                auto message = boost::format("Failed to setresuid(%1%, %1%, %1%): %2%") % uid % strerror(errno);
                SARUS_THROW_ERROR(message.str());
            }
        };

        return common::forkExecWait(args, std::function<void()>{setUserIdentity}) == 0;
    }

    void verifyThatImageIsAvailable() const {
        cli::utility::printLog( boost::format("Verifying that image %s is available") % conf->imageReference,
                                common::LogLevel::INFO);
        // switch to user filesystem identity to make sure we can access images on root_squashed filesystems
        auto rootIdentity = common::UserIdentity{};
        common::setFilesystemUid(conf->userIdentity);

        try {
            auto imageStore = image_manager::ImageStore(conf);
            auto image = imageStore.findImage(conf->imageReference);
            if(!image && conf->imageReference.server == common::ImageReference::DEFAULT_SERVER) {
                auto message = boost::format("Image %s is not available. Attempting to look for equivalent image in %s server repositories")
                                             % conf->imageReference % common::ImageReference::LEGACY_DEFAULT_SERVER;
                cli::utility::printLog(message.str(), common::LogLevel::GENERAL, std::cerr);
                conf->imageReference.server = common::ImageReference::LEGACY_DEFAULT_SERVER;
                image = imageStore.findImage(conf->imageReference);
            }
            if(!image) {
                auto message = boost::format("Image %s is not available") % conf->imageReference;
                cli::utility::printLog(message.str(), common::LogLevel::GENERAL, std::cerr);
                exit(EXIT_FAILURE);
            }
            verifyImageBackingFiles(*image);
        } catch(const std::exception& e) {
            SARUS_RETHROW_ERROR(e, "Failed to verify that image is available");
        }

        common::setFilesystemUid(rootIdentity);

        cli::utility::printLog(boost::format("Successfully verified that image %s is available") % conf->imageReference,
                               common::LogLevel::INFO);
    }

    void verifyImageBackingFiles(const common::SarusImage& image) const {
        auto missing = std::vector<std::string>{};
        if(!boost::filesystem::exists(image.imageFile)) {
            missing.push_back(image.imageFile.string());
        }
        if(!boost::filesystem::exists(image.metadataFile)) {
            missing.push_back(image.metadataFile.string());
        }
        if(!missing.empty()) {
            auto message = boost::format("Storage inconsistency detected: image %s is listed in the repository "
                                         "metadata but the following backing files are missing:\n%s\n"
                                         "Attempting to remove image from metadata to resolve the inconsistency.\n")
                                         % conf->imageReference % boost::algorithm::join(missing, "\n");
            cli::utility::printLog(message.str(), common::LogLevel::GENERAL, std::cerr);

            auto imageManager = image_manager::ImageManager{conf};
            imageManager.removeImage();

            cli::utility::printLog("Please pull or load the image again.", common::LogLevel::GENERAL, std::cerr);
            exit(EXIT_FAILURE);
        }
    }

private:
    boost::program_options::options_description optionsDescription{"Options"};
    std::vector<std::string> env;
    std::shared_ptr<common::Config> conf;
    std::vector<std::string> deviceMounts;
    std::string entrypoint;
    std::string pid;
    std::string workdir;
};

}
}

#endif
