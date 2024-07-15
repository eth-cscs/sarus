/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
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

#include "common/Config.hpp"
#include "libsarus/CLIArguments.hpp"
#include "libsarus/DeviceParser.hpp"
#include "libsarus/MountParser.hpp"
#include "libsarus/Utility.hpp"
#include "cli/Utility.hpp"
#include "cli/Command.hpp"
#include "cli/HelpMessage.hpp"
#include "image_manager/ImageStore.hpp"
#include "runtime/Runtime.hpp"
#include "libsarus/DeviceMount.hpp"


namespace sarus {
namespace cli {

class CommandRun : public Command {
public:
    CommandRun() {
        initializeOptionsDescription();
    }

    CommandRun(const libsarus::CLIArguments& args, std::shared_ptr<common::Config> conf)
        : conf{std::move(conf)}
    {
        initializeOptionsDescription();
        parseCommandArguments(args);
    }

    void execute() override {
        cli::utility::printLog("Executing run command", libsarus::LogLevel::INFO);

        if(conf->commandRun.enableSSH && !checkUserHasSshKeys()) {
            auto message = boost::format("Failed to check the SSH keys. Hint: try to"
                                         " generate the SSH keys with 'sarus ssh-keygen'.");
            libsarus::Logger::getInstance().log(message, "CLI", libsarus::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
        }

        verifyThatImageIsAvailable();

        auto setupBegin = std::chrono::high_resolution_clock::now();
        auto cliTime = std::chrono::duration<double>(setupBegin - conf->program_start);
        auto message = boost::format("Processed CLI arguments in %.6f seconds") % cliTime.count();
        cli::utility::printLog(message, libsarus::LogLevel::INFO);

        auto runtime = runtime::Runtime{conf};
        runtime.setupOCIBundle();

        auto setupEnd = std::chrono::high_resolution_clock::now();
        auto setupTime = std::chrono::duration<double>(setupEnd - setupBegin);
        message = boost::format("Successfully set up container in %.6f seconds") % setupTime.count();
        cli::utility::printLog(message, libsarus::LogLevel::INFO);

        runtime.executeContainer();

        cli::utility::printLog("Successfully executed run command", libsarus::LogLevel::INFO);
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
            ("annotation",
                boost::program_options::value<std::vector<std::string>>(&annotations),
                "Add an OCI annotation to the container")
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
            ("mpi,m", "Enable native MPI support. Implies '--glibc'")
            ("mpi-type",
                boost::program_options::value<std::string>(&mpiType),
                "Enable MPI support for a specific MPI implementation. If no value is supplied, "
                "Sarus will use the default configured by the administrator. "
                "Implies '--mpi' and '--glibc'")
            ("name,n",
                boost::program_options::value<std::string>(&containerName),
                "Assign a name to the container"
            )
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

    void parseCommandArguments(const libsarus::CLIArguments& args) {
        cli::utility::printLog("parsing CLI arguments of run command", libsarus::LogLevel::DEBUG);

        libsarus::CLIArguments nameAndOptionArgs, positionalArgs;
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
            conf->commandRun.execArgs = libsarus::CLIArguments(positionalArgs.begin()+1, positionalArgs.end());

            if(values.count("entrypoint")) {
                if(entrypoint.empty()) {
                    conf->commandRun.entrypoint = libsarus::CLIArguments{};
                }
                else {
                    std::vector<std::string> entrypointArgs;
                    boost::algorithm::split(entrypointArgs, entrypoint, boost::algorithm::is_space());
                    conf->commandRun.entrypoint = libsarus::CLIArguments{entrypointArgs.cbegin(), entrypointArgs.cend()};
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

            if(values.count("mpi-type")) {
                conf->commandRun.useMPI = true;
                if(mpiType.empty()) {
                    SARUS_THROW_ERROR("Empty value provided for --mpi-type option");
                }
                else{
                    conf->commandRun.mpiType = mpiType;
                }
            }
            else if(values.count("mpi")) {
                conf->commandRun.useMPI = true;
                if(conf->json.HasMember("defaultMPIType")) {
                    conf->commandRun.mpiType = conf->json["defaultMPIType"].GetString();
                }
            }
            else {
                conf->commandRun.useMPI = false;
            }
            if(values.count("name")) {
                conf->commandRun.containerName = containerName;
                cli::utility::printLog("name of container: " + containerName, libsarus::LogLevel::DEBUG);
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
            cli::utility::printLog(message, libsarus::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
        }

        makeAnnotations();
        makeUserEnvironment();
        makeSiteMountObjects();
        makeUserMountObjects();
        makeSiteDeviceMountObjects();
        makeUserDeviceMountObjects();

        cli::utility::printLog("successfully parsed CLI arguments", libsarus::LogLevel::DEBUG);
    }

    void makeAnnotations() {
        for(const auto& annotation : annotations) {
            auto message = boost::format("Parsing annotation from CLI '%s'") % annotation;
            cli::utility::printLog(message, libsarus::LogLevel::DEBUG);

            if(annotation.empty()) {
                auto message = boost::format("Invalid annotation requested from CLI: empty option value");
                utility::printLog(message, libsarus::LogLevel::GENERAL, std::cerr);
                SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
            }

            std::string key, value;
            try {
                std::tie(key, value) = libsarus::string::parseKeyValuePair(annotation);
            }
            catch(std::exception& e) {
                auto message = boost::format("Error parsing annotation from CLI '%s': %s")
                               % annotation % e.what();
                cli::utility::printLog(message, libsarus::LogLevel::GENERAL, std::cerr);
                SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
            }

            conf->commandRun.ociAnnotations[key] = value;
            message = boost::format("Successfully parsed annotation from CLI: Key: '%s' - Value: '%s'")
                      % key % value;
            cli::utility::printLog(message, libsarus::LogLevel::DEBUG);
        }
    }

    void makeUserEnvironment() {
        for(const auto& variable : env) {
            auto message = boost::format("Parsing environment variable requested from CLI '%s'") % variable;
            cli::utility::printLog(message, libsarus::LogLevel::DEBUG);

            if(variable.empty()) {
                auto message = boost::format("Invalid environment variable requested from CLI: empty option value");
                utility::printLog(message, libsarus::LogLevel::GENERAL, std::cerr);
                SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
            }

            std::string name, value;

            if(std::find(variable.cbegin(), variable.cend(), '=') == variable.cend()) {
                auto message = boost::format("Environment variable requested from CLI '%s' does not feature '=' separator. "
                                             "Treating string as variable name and attempting to source value from host "
                                             "environment") % variable;
                cli::utility::printLog(message, libsarus::LogLevel::INFO);

                auto& hostEnvironment = conf->commandRun.hostEnvironment;
                auto hostVariable = hostEnvironment.find(variable);
                if (hostVariable != hostEnvironment.cend()) {
                    name = hostVariable->first;
                    value = hostVariable->second;
                }
                else {
                    auto message = boost::format("Environment variable requested from CLI '%s' does not correspond to a variable "
                                                 "present in the host environment. Skipping request.") % variable;
                    cli::utility::printLog(message, libsarus::LogLevel::INFO);
                    continue;
                }
            }
            else {
                try {
                    std::tie(name, value) = libsarus::environment::parseVariable(variable);
                }
                catch(std::exception& e) {
                    auto message = boost::format("Error parsing environment variable requested from CLI '%s': %s")
                                   % variable % e.what();
                    cli::utility::printLog(message, libsarus::LogLevel::GENERAL, std::cerr);
                    SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
                }
            }

            conf->commandRun.userEnvironment[name] = value;
            message = boost::format("Successfully parsed environment variable from CLI: Name: '%s' - Value: '%s'")
                      % name % value;
            cli::utility::printLog(message, libsarus::LogLevel::DEBUG);
        }
    }

    void makeSiteMountObjects() {
        auto parser = libsarus::MountParser{conf->getRootfsDirectory(), conf->userIdentity};
        for(const auto& map : convertJSONSiteMountsToMaps()) {
            conf->commandRun.mounts.push_back(parser.parseMountRequest(map));
        }
    }

    void makeUserMountObjects() {
        auto parser = libsarus::MountParser{conf->getRootfsDirectory(), conf->userIdentity};
        if (conf->json.HasMember("userMounts")) {  
            parser = libsarus::MountParser{conf->getRootfsDirectory(), conf->userIdentity, conf->json["userMounts"]};
        }
        for (const auto& mountString : conf->commandRun.userMounts) {
            auto map = libsarus::string::parseMap(mountString);
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
        }
        catch(const std::exception& e) {
            SARUS_RETHROW_ERROR(e, "Failed to convert JSON mount entry to map");
        }

        return maps;
    }

    void makeSiteDeviceMountObjects() {
        auto parser = libsarus::DeviceParser{conf->getRootfsDirectory(), conf->userIdentity};
        for (const auto& requestString : convertJSONSiteDevicesToStrings()) {
            try {
                conf->commandRun.deviceMounts.push_back(parser.parseDeviceRequest(requestString));
            }
            catch (const libsarus::Error& e) {
                auto message = boost::format("Error while processing the 'siteDevices' parameter in the configuration file. "
                    "Please contact your system administrator");
                cli::utility::printLog(message, libsarus::LogLevel::GENERAL, std::cerr);
                SARUS_RETHROW_ERROR(e, message.str(), libsarus::LogLevel::INFO);
            }
        }
    }

    void makeUserDeviceMountObjects() {
        auto parser = libsarus::DeviceParser{conf->getRootfsDirectory(), conf->userIdentity};
        auto siteDevices = conf->commandRun.deviceMounts;
        for (const auto& requestString : deviceMounts) {
            auto deviceMount = std::move(parser.parseDeviceRequest(requestString));
            auto previousSiteDevice = findMatchingSiteDevice(deviceMount, siteDevices);
            if (previousSiteDevice) {
                auto message = boost::format("Device %s already added by the system administrator at container path %s with access %s. "
                                             "Skipping request from the command line")
                    % deviceMount->getSource() % previousSiteDevice->getDestination() % previousSiteDevice->getAccess().string();
                utility::printLog(message, libsarus::LogLevel::WARN, std::cerr);
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

    std::shared_ptr<libsarus::DeviceMount> findMatchingSiteDevice(const std::unique_ptr<libsarus::DeviceMount>& deviceMount,
                                                                 const std::vector<std::shared_ptr<libsarus::DeviceMount>> siteDevices) const {
        for (const auto& siteDevice : siteDevices) {
            if (deviceMount->getSource() == siteDevice->getSource()) {
                return siteDevice;
            }
        }
        return std::shared_ptr<libsarus::DeviceMount>{};
    }

    bool checkUserHasSshKeys() const {
        cli::utility::printLog( "Checking that the user has SSH keys",
                                libsarus::LogLevel::INFO);

        libsarus::environment::setVariable("HOOK_BASE_DIR", conf->json["localRepositoryBaseDir"].GetString());

        auto passwdFile = boost::filesystem::path{ conf->json["prefixDir"].GetString() } / "etc/passwd";
        libsarus::environment::setVariable("PASSWD_FILE", passwdFile.string());

        auto args = libsarus::CLIArguments{
            std::string{ conf->json["prefixDir"].GetString() } + "/bin/ssh_hook",
            "check-user-has-sshkeys"
        };
        if(libsarus::Logger::getInstance().getLevel() == libsarus::LogLevel::INFO) {
            args.push_back("--verbose");
        }
        else if(libsarus::Logger::getInstance().getLevel() == libsarus::LogLevel::DEBUG) {
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

        return libsarus::process::forkExecWait(args, std::function<void()>{setUserIdentity}) == 0;
    }

    void verifyThatImageIsAvailable() const {
        cli::utility::printLog( boost::format("Verifying that image %s is available") % conf->imageReference,
                                libsarus::LogLevel::INFO);
        // switch to user identity to make sure that:
        //   - we can access images on root_squashed filesystems
        //   - we do not create/update local repo files (e.g. repo metadata and lockfiles) with root ownership
        auto rootIdentity = libsarus::UserIdentity{};
        libsarus::process::switchIdentity(conf->userIdentity);

        try {
            auto imageStore = image_manager::ImageStore(conf);
            auto image = imageStore.findImage(conf->imageReference);
            if(!image && conf->imageReference.server == common::ImageReference::DEFAULT_SERVER) {
                auto message = boost::format("Image %s is not available. Attempting to look for equivalent image in %s server repositories")
                                             % conf->imageReference % common::ImageReference::LEGACY_DEFAULT_SERVER;
                cli::utility::printLog(message.str(), libsarus::LogLevel::GENERAL, std::cerr);
                conf->imageReference.server = common::ImageReference::LEGACY_DEFAULT_SERVER;
                image = imageStore.findImage(conf->imageReference);
            }
            if(!image) {
                auto message = boost::format("Image %s is not available") % conf->imageReference;
                cli::utility::printLog(message.str(), libsarus::LogLevel::GENERAL, std::cerr);
                exit(EXIT_FAILURE);
            }
        }
        catch(const std::exception& e) {
            SARUS_RETHROW_ERROR(e, "Failed to verify that image is available");
        }

        libsarus::process::switchIdentity(rootIdentity);

        cli::utility::printLog(boost::format("Successfully verified that image %s is available") % conf->imageReference,
                               libsarus::LogLevel::INFO);
    }

private:
    boost::program_options::options_description optionsDescription{"Options"};
    std::shared_ptr<common::Config> conf;
    std::vector<std::string> annotations;
    std::vector<std::string> deviceMounts;
    std::vector<std::string> env;
    std::string entrypoint;
    std::string mpiType;
    std::string containerName;
    std::string pid;
    std::string workdir;
};

}
}

#endif
