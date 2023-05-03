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

#include "common/Logger.hpp"
#include "common/Error.hpp"
#include "common/Utility.hpp"
#include "cli/MountParser.hpp"
#include "cli/DeviceParser.hpp"
#include "hooks/common/Utility.hpp"
#include "runtime/mount_utilities.hpp"

namespace sarus {
namespace hooks {
namespace mount {

MountHook::MountHook(const sarus::common::CLIArguments& args) {
    log("Initializing hook", sarus::common::LogLevel::INFO);

    std::tie(bundleDir, pidOfContainer) = hooks::common::utility::parseStateOfContainerFromStdin();
    hooks::common::utility::enterMountNamespaceOfProcess(pidOfContainer);
    parseConfigJSONOfBundle();
    parseEnvironmentVariables();
    parseCliArguments(args);

    log("Successfully initialized hook", sarus::common::LogLevel::INFO);
}

void MountHook::parseConfigJSONOfBundle() {
    log("Parsing bundle's config.json", sarus::common::LogLevel::INFO);

    auto json = sarus::common::readJSON(bundleDir / "config.json");

    hooks::common::utility::applyLoggingConfigIfAvailable(json);

    auto root = boost::filesystem::path{ json["root"]["path"].GetString() };
    if(root.is_absolute()) {
        rootfsDir = root;
    }
    else {
        rootfsDir = bundleDir / root;
    }

    uid_t uidOfUser = json["process"]["user"]["uid"].GetInt();
    gid_t gidOfUser = json["process"]["user"]["gid"].GetInt();
    userIdentity = sarus::common::UserIdentity(uidOfUser, gidOfUser, {});

    auto fiProviderPathEnv = hooks::common::utility::getEnvironmentVariableValueFromOCIBundle("FI_PROVIDER_PATH", bundleDir);
    if (fiProviderPathEnv && !(*fiProviderPathEnv).empty()) {
        fiProviderPath = *fiProviderPathEnv;
        auto message = boost::format("Found FI_PROVIDER_PATH in the container's environment: %s") % fiProviderPath;
        log(message, sarus::common::LogLevel::INFO);
    }

    log("Successfully parsed bundle's config.json", sarus::common::LogLevel::INFO);
}

void MountHook::parseEnvironmentVariables() {
    log("Parsing environment variables", sarus::common::LogLevel::INFO);
    try{
        ldconfigPath = sarus::common::getEnvironmentVariable("LDCONFIG_PATH");
    }
    catch (sarus::common::Error& e) {}
    log("Successfully parsed environment variables", sarus::common::LogLevel::INFO);
}

void MountHook::parseCliArguments(const sarus::common::CLIArguments& args) {
    log("Parsing CLI arguments", sarus::common::LogLevel::INFO);

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

    auto mountParser = sarus::cli::MountParser{rootfsDir, userIdentity};
    for (const auto& mountString : inputMounts) {
        auto parseCandidate = replaceStringWildcards(mountString);
        auto map = sarus::common::parseMap(parseCandidate);
        bindMounts.push_back(mountParser.parseMountRequest(map));
    }

    auto deviceParser = cli::DeviceParser{rootfsDir, userIdentity};
    for (const auto& deviceString : cliDeviceMounts) {
        deviceMounts.push_back(deviceParser.parseDeviceRequest(deviceString));
    }

    log("Successfully parsed CLI arguments", sarus::common::LogLevel::INFO);
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
    log(boost::format("Replacing <FI_PROVIDER_PATH> wildcard in '%s'") % input, sarus::common::LogLevel::DEBUG);
    if (fiProviderPath.empty()) {
        try {
            // The default libfabric search path for external providers is "<libdir>/libfabric".
            // E.g. if the library is installed at /usr/lib/libfabric.so.1, the search path is /usr/lib/libfabric
            fiProviderPath = findLibfabricLibdir() / "libfabric";
        }
        catch (const sarus::common::Error& e) {
            fiProviderPath = boost::filesystem::path("/usr/lib");
        }
        auto message = boost::format("Resolved <FI_PROVIDER_PATH> wildcard to %s") % fiProviderPath;
        log(message, sarus::common::LogLevel::INFO);
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
        log(message, sarus::common::LogLevel::INFO);
        SARUS_THROW_ERROR(message, sarus::common::LogLevel::INFO);
    }
    auto containerLibPaths = sarus::common::getSharedLibsFromDynamicLinker(ldconfigPath, rootfsDir);
    boost::smatch match;
    for (const auto& p : containerLibPaths){
        if (boost::regex_search(p.string(), match, boost::regex("libfabric\\.so(?:\\.\\d+)+$"))
                && boost::filesystem::exists(rootfsDir / sarus::common::realpathWithinRootfs(rootfsDir, p))) {
            auto message = boost::format("Found existing libfabric from the container's dynamic linker cache: %s") % p;
            log(message, sarus::common::LogLevel::DEBUG);
            return p.parent_path();
        }
    }
    std::string message("Failed to find existing libfabric path in the container's dynamic linker cache");
    log(message, sarus::common::LogLevel::INFO);
    SARUS_THROW_ERROR(message, sarus::common::LogLevel::INFO);
}

void MountHook::performBindMounts() const {
    if (bindMounts.empty()) {
        log("No bind mounts to perform", sarus::common::LogLevel::INFO);
        return;
    }

    log("Performing bind mounts", sarus::common::LogLevel::INFO);
    for(const auto& mount : bindMounts) {
        mount->performMount();
    }
    log("Successfully performed bind mounts", sarus::common::LogLevel::INFO);
}

void MountHook::performDeviceMounts() const {
    if (deviceMounts.empty()) {
        log("No device mounts to perform", sarus::common::LogLevel::INFO);
        return;
    }

    log("Performing device mounts", sarus::common::LogLevel::INFO);
    auto devicesCgroupPath = common::utility::findCgroupPath("devices", "/", pidOfContainer);
    for(const auto& mount : deviceMounts) {
        mount->performMount();
        common::utility::whitelistDeviceInCgroup(devicesCgroupPath, rootfsDir / mount->destination);
    }
    log("Successfully performed device mounts", sarus::common::LogLevel::INFO);
}

void MountHook::activate() const {
    performBindMounts();
    performDeviceMounts();
    if (!ldconfigPath.empty()) {
        log("Updating container's dynamic linker cache", sarus::common::LogLevel::INFO);
        sarus::common::executeCommand(ldconfigPath.string() + " -r " + rootfsDir.string());
    }
}

void MountHook::log(const std::string& message, sarus::common::LogLevel level) const {
    sarus::common::Logger::getInstance().log(message, "Mount hook", level);
}

void MountHook::log(const boost::format& message, sarus::common::LogLevel level) const {
    sarus::common::Logger::getInstance().log(message.str(), "Mount hook", level);
}

}}} // namespace
