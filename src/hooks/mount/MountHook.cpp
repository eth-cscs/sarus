/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "hooks/mount/MountHook.hpp"

#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <rapidjson/document.h>

#include "libsarus/Logger.hpp"
#include "libsarus/Error.hpp"
#include "libsarus/Utility.hpp"
#include "libsarus/MountParser.hpp"
#include "libsarus/DeviceParser.hpp"

namespace sarus {
namespace hooks {
namespace mount {

MountHook::MountHook(const libsarus::CLIArguments& args) {
    log("Initializing hook", libsarus::LogLevel::INFO);

    containerState = libsarus::hook::parseStateOfContainerFromStdin();
    parseConfigJSONOfBundle();
    parseEnvironmentVariables();
    parseCliArguments(args);

    log("Successfully initialized hook", libsarus::LogLevel::INFO);
}

void MountHook::parseConfigJSONOfBundle() {
    log("Parsing bundle's config.json", libsarus::LogLevel::INFO);

    auto json = libsarus::json::read(containerState.bundle() / "config.json");

    libsarus::hook::applyLoggingConfigIfAvailable(json);

    auto root = boost::filesystem::path{ json["root"]["path"].GetString() };
    if(root.is_absolute()) {
        rootfsDir = root;
    }
    else {
        rootfsDir = containerState.bundle() / root;
    }

    uid_t uidOfUser = json["process"]["user"]["uid"].GetInt();
    gid_t gidOfUser = json["process"]["user"]["gid"].GetInt();
    userIdentity = libsarus::UserIdentity(uidOfUser, gidOfUser, {});

    auto fiProviderPathEnv = libsarus::hook::getEnvironmentVariableValueFromOCIBundle("FI_PROVIDER_PATH", containerState.bundle());
    if (fiProviderPathEnv && !(*fiProviderPathEnv).empty()) {
        fiProviderPath = *fiProviderPathEnv;
        auto message = boost::format("Found FI_PROVIDER_PATH in the container's environment: %s") % fiProviderPath;
        log(message, libsarus::LogLevel::INFO);
    }

    log("Successfully parsed bundle's config.json", libsarus::LogLevel::INFO);
}

void MountHook::parseEnvironmentVariables() {
    log("Parsing environment variables", libsarus::LogLevel::INFO);
    try{
        ldconfigPath = libsarus::environment::getVariable("LDCONFIG_PATH");
    }
    catch (libsarus::Error& e) {}
    log("Successfully parsed environment variables", libsarus::LogLevel::INFO);
}

void MountHook::parseCliArguments(const libsarus::CLIArguments& args) {
    log("Parsing CLI arguments", libsarus::LogLevel::INFO);

    std::vector<std::string> inputMounts, cliDeviceMounts;
    boost::program_options::options_description optionsDescription{"Options"};
    optionsDescription.add_options()
        ("device",
            boost::program_options::value<std::vector<std::string>>(&cliDeviceMounts),
            "Mount devices into the container")
        ("mount",
            boost::program_options::value<std::vector<std::string>>(&inputMounts),
            "Mount files and directories into the container");

    boost::program_options::variables_map values;
    boost::program_options::parsed_options parsed =
        boost::program_options::command_line_parser(args.argc(), args.argv())
            .options(optionsDescription)
            .style(boost::program_options::command_line_style::unix_style)
            .run();
    boost::program_options::store(parsed, values);
    boost::program_options::notify(values);

    auto mountParser = libsarus::MountParser{rootfsDir, userIdentity};
    for (const auto& mountString : inputMounts) {
        auto parseCandidate = replaceStringWildcards(mountString);
        auto map = libsarus::string::parseMap(parseCandidate);
        bindMounts.push_back(mountParser.parseMountRequest(map));
    }

    auto deviceParser = libsarus::DeviceParser{rootfsDir, userIdentity};
    for (const auto& deviceString : cliDeviceMounts) {
        deviceMounts.push_back(deviceParser.parseDeviceRequest(deviceString));
    }

    log("Successfully parsed CLI arguments", libsarus::LogLevel::INFO);
}

std::string MountHook::replaceStringWildcards(const std::string& input) {
    auto ret = input;
    boost::smatch match;
    if (boost::regex_search(input, match, boost::regex("<FI_PROVIDER_PATH>"))) {
        ret = replaceFiProviderPathWildcard(ret);
    }
    return ret;
}

std::string MountHook::replaceFiProviderPathWildcard(const std::string& input) {
    log(boost::format("Replacing <FI_PROVIDER_PATH> wildcard in '%s'") % input, libsarus::LogLevel::DEBUG);
    if (fiProviderPath.empty()) {
        try {
            // The default libfabric search path for external providers is "<libdir>/libfabric".
            // E.g. if the library is installed at /usr/lib/libfabric.so.1, the search path is /usr/lib/libfabric
            fiProviderPath = findLibfabricLibdir() / "libfabric";
        }
        catch (const libsarus::Error& e) {
            fiProviderPath = boost::filesystem::path("/usr/lib");
        }
        auto message = boost::format("Resolved <FI_PROVIDER_PATH> wildcard to %s") % fiProviderPath;
        log(message, libsarus::LogLevel::INFO);
    }
    return boost::regex_replace(input, boost::regex("<FI_PROVIDER_PATH>"), fiProviderPath.string());
}

/**
 * Returns the installation path of the libfabric.so.* shared library in the container. *
 * Notice the returned path is NOT the "installation prefix", that is the path under which a full
 * libfabric installation is performed (thus having subdirectories like bin/, include/, lib/, etc.),
 * but actually represents the directory of the shared library itself. As such, it is correctly described
 * by the "libdir" term.
 */
boost::filesystem::path MountHook::findLibfabricLibdir() const {
    if (ldconfigPath.empty()) {
        std::string message("Failed to find existing libfabric path in the container's dynamic linker cache:"
                            " no ldconfig path configured for the hook");
        log(message, libsarus::LogLevel::INFO);
        SARUS_THROW_ERROR(message, libsarus::LogLevel::INFO);
    }
    auto containerLibPaths = libsarus::sharedlibs::getListFromDynamicLinker(ldconfigPath, rootfsDir);
    boost::smatch match;
    for (const auto& p : containerLibPaths){
        if (boost::regex_search(p.string(), match, boost::regex("libfabric\\.so(?:\\.\\d+)+$"))
                && boost::filesystem::exists(rootfsDir / libsarus::filesystem::realpathWithinRootfs(rootfsDir, p))) {
            auto message = boost::format("Found existing libfabric from the container's dynamic linker cache: %s") % p;
            log(message, libsarus::LogLevel::DEBUG);
            return p.parent_path();
        }
    }
    std::string message("Failed to find existing libfabric path in the container's dynamic linker cache");
    log(message, libsarus::LogLevel::INFO);
    SARUS_THROW_ERROR(message, libsarus::LogLevel::INFO);
}

void MountHook::performBindMounts() const {
    if (bindMounts.empty()) {
        log("No bind mounts to perform", libsarus::LogLevel::INFO);
        return;
    }

    log("Performing bind mounts", libsarus::LogLevel::INFO);
    for(const auto& mount : bindMounts) {
        mount->performMount();
    }
    log("Successfully performed bind mounts", libsarus::LogLevel::INFO);
}

void MountHook::performDeviceMounts() const {
    if (deviceMounts.empty()) {
        log("No device mounts to perform", libsarus::LogLevel::INFO);
        return;
    }

    log("Performing device mounts", libsarus::LogLevel::INFO);
    auto devicesCgroupPath = libsarus::hook::findCgroupPath("devices", "/", containerState.pid());
    for(const auto& mount : deviceMounts) {
        mount->performMount();
        libsarus::hook::whitelistDeviceInCgroup(devicesCgroupPath, rootfsDir / mount->getDestination());
    }
    log("Successfully performed device mounts", libsarus::LogLevel::INFO);
}

void MountHook::activate() const {
    performBindMounts();
    performDeviceMounts();
    if (!ldconfigPath.empty()) {
        log("Updating container's dynamic linker cache", libsarus::LogLevel::INFO);
        libsarus::process::executeCommand(ldconfigPath.string() + " -r " + rootfsDir.string());
    }
}

void MountHook::log(const std::string& message, libsarus::LogLevel level) const {
    libsarus::Logger::getInstance().log(message, "Mount hook", level);
}

void MountHook::log(const boost::format& message, libsarus::LogLevel level) const {
    libsarus::Logger::getInstance().log(message.str(), "Mount hook", level);
}

}}} // namespace
