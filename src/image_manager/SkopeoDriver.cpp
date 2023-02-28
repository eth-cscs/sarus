/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "image_manager/SkopeoDriver.hpp"

#include <chrono>

#include <rapidjson/document.h>
#include <boost/regex.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "common/Error.hpp"
#include "common/PathRAII.hpp"
#include "common/Utility.hpp"
#include "image_manager/Utility.hpp"


namespace sarus {
namespace image_manager {

SkopeoDriver::SkopeoDriver(std::shared_ptr<const common::Config> config)
    : skopeoPath{config->json["skopeoPath"].GetString()},
      tempDir{config->directories.temp},
      cachePath{config->directories.cache}
{
    if (!boost::filesystem::is_regular_file(skopeoPath)) {
        auto message = boost::format("The path to the Skopeo executable '%s' configured in sarus.json does not "
                                     "lead to a regular file. "
                                     "Please contact your system administrator.") % skopeoPath;
        SARUS_THROW_ERROR(message.str());
    }

    authFileBasePath = config->directories.repository;
    try {
        auto xdgRuntimePath = boost::filesystem::path{common::getEnvironmentVariable("XDG_RUNTIME_DIR")};
        if (boost::filesystem::is_directory(xdgRuntimePath)) {
            authFileBasePath = xdgRuntimePath / "sarus";
        }
        else {
            printLog(boost::format("XDG_RUNTIME_DIR environment set to %s, but directory does not exist") % xdgRuntimePath,
                     common::LogLevel::DEBUG);
        }
    }
    catch (common::Error& e) {}  // common::getEnvironmentVariable() throws if searched variable is not set
    printLog( boost::format("Set authentication file base path to %s") % authFileBasePath, common::LogLevel::DEBUG);

    authFilePath.clear();

    if (const rapidjson::Value* configPolicy = rapidjson::Pointer("/containersPolicy/path").Get(config->json)) {
        if (!boost::filesystem::is_regular_file(configPolicy->GetString())) {
            auto message = boost::format("Custom containers policy file '%s' configured in sarus.json is not a regular file. "
                                         "Please contact your system administrator.\nDocumentation reference: "
                                         "https://sarus.readthedocs.io/en/stable/config/configuration_reference.html"
                                         "#containerspolicy-object-optional") % configPolicy->GetString();
            SARUS_THROW_ERROR(message.str());
        }
        customPolicyPath = boost::filesystem::path(configPolicy->GetString());
    }

    if (const rapidjson::Value* configEnforcePolicy = rapidjson::Pointer("/containersPolicy/enforce").Get(config->json)) {
        enforceCustomPolicy = configEnforcePolicy->GetBool();
    }
    else {
        enforceCustomPolicy = false;
    }

    if (const rapidjson::Value* configRegistriesD = rapidjson::Pointer("/containersRegistries.dPath").Get(config->json)) {
        if (!boost::filesystem::is_directory(configRegistriesD->GetString())) {
            auto message = boost::format("Custom containers registries.d path '%s' configured in sarus.json is not a directory. "
                                         "Please contact your system administrator.\nDocumentation reference: "
                                         "https://sarus.readthedocs.io/en/stable/config/configuration_reference.html"
                                         "#containersregistries-dpath-string-optional")
                                        % configRegistriesD->GetString();
            SARUS_THROW_ERROR(message.str());
        }
        customRegistriesDPath = boost::filesystem::path(configRegistriesD->GetString());
    }
}

SkopeoDriver::~SkopeoDriver() {
    boost::filesystem::remove(authFilePath);
}

boost::filesystem::path SkopeoDriver::copyToOCIImage(const std::string& sourceTransport, const std::string& sourceReference) const {
    printLog( boost::format("Copying '%s' to OCI image") % sourceReference, common::LogLevel::INFO);

    auto ociImagePath = common::makeUniquePathWithRandomSuffix(cachePath / "ociImages/image");
    auto ociImageRAII = common::PathRAII{ociImagePath};
    printLog( boost::format("Creating temporary OCI image in: %s") % ociImagePath, common::LogLevel::DEBUG);
    common::createFoldersIfNecessary(ociImagePath);

    if (sourceTransport == "docker") {
        auto imageBlobsPath = ociImagePath / "blobs";
        boost::filesystem::create_symlink(cachePath/"blobs", imageBlobsPath);
        printLog( boost::format("Symlinking blob cache %s to %s") % cachePath % imageBlobsPath, common::LogLevel::DEBUG);
    }

    auto args = generateBaseArgs();
    args.push_back("copy");
    if (!authFilePath.empty()){
        args += common::CLIArguments{"--src-authfile", authFilePath.string()};
    }
    args += common::CLIArguments{getTransportString(sourceTransport) + sourceReference,
                                 "oci:"+ociImagePath.string()+":sarus-oci-image"};

    auto start = std::chrono::system_clock::now();
    auto status = common::forkExecWait(args);
    if(status != 0) {
        auto message = boost::format("Failed to copy '%s' to OCI image") % sourceReference;
        SARUS_THROW_ERROR(message.str());
    }
    auto end = std::chrono::system_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / double(1000);
    printLog(boost::format("Elapsed time on copy operation: %s [sec]") % elapsed, common::LogLevel::INFO);
    printLog(boost::format("Successfully created OCI image"), common::LogLevel::INFO);

    ociImageRAII.release();
    return ociImagePath;
}
std::string SkopeoDriver::inspectRaw(const std::string& sourceTransport, const std::string& sourceReference) const {
    auto inspectOutput = std::string{};

    auto args = generateBaseArgs() + common::CLIArguments{"inspect", "--raw"};
    if (!authFilePath.empty()){
        args += common::CLIArguments{"--authfile", authFilePath.string()};
    }
    args.push_back(getTransportString(sourceTransport) + sourceReference);

    auto start = std::chrono::system_clock::now();
    try {
        inspectOutput = common::executeCommand(args.string());
    }
    catch(common::Error& e) {
        // Confirm skopeo failed because of image non existent or unauthorized access
        auto errorMessage = std::string(e.what());
        auto exitCodeString = std::string{"Process terminated with status 1"};

        /**
         * Registries often respond differently to the same incorrect requests, making it very hard to
         * consistently understand whether an image is not present in the registry or it's just private.
         * For example, Docker Hub responds both "denied" and "unauthorized", regardless if the image is private or non-existent;
         * Quay.io responds "unauthorized" regardless if the image is private or non-existent.
         * Additionally, the error strings might have different contents depending on the registry.
         */
        auto deniedAccessString = std::string{"denied:"};
        auto unauthorizedAccessString = std::string{"unauthorized:"};
        auto invalidCredentialsString = std::string{"invalid username/password:"};
        auto manifestErrorString = std::string{"reading manifest"};

        if(errorMessage.find(exitCodeString) != std::string::npos) {
            auto messageHeader = boost::format{"Failed to pull image '%s'"} % sourceReference;
            printLog(messageHeader, common::LogLevel::GENERAL, std::cerr);

            if(errorMessage.find(invalidCredentialsString) != std::string::npos) {
                auto message = boost::format{"Unable to retrieve auth token: invalid username or password provided."};
                printLog(message, common::LogLevel::GENERAL, std::cerr);
                SARUS_THROW_ERROR(errorMessage, common::LogLevel::INFO);
            }

            if(errorMessage.find(manifestErrorString) != std::string::npos) {
                printLog("Error reading manifest from registry.", common::LogLevel::GENERAL, std::cerr);
            }

            if(errorMessage.find(unauthorizedAccessString) != std::string::npos
               || errorMessage.find(deniedAccessString) != std::string::npos) {
                auto message = boost::format{"The image may be private or not present in the remote registry."
                                             "\nDid you perform a login with the proper credentials?"
                                             "\nSee 'sarus help pull' (--login option)"};
                printLog(message, common::LogLevel::GENERAL, std::cerr);
            }

            SARUS_THROW_ERROR(errorMessage, common::LogLevel::INFO);
        }
        else {
            SARUS_RETHROW_ERROR(e, "Error accessing image in the remote registry.");
        }
    }
    auto end = std::chrono::system_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / double(1000);
    printLog(boost::format("Elapsed time on raw inspect operation: %s [sec]") % elapsed, common::LogLevel::INFO);

    // The Skopeo debug/warning messages are useful to be embedded in an exception message,
    // but prevent the output from being converted to JSON.
    // Exclude the extra lines from Skopeo and only return the JSON output
    inspectOutput = inspectOutput.substr(inspectOutput.rfind("\n{")+1);

    printLog(boost::format("Raw inspect filtered output: %s") % inspectOutput, common::LogLevel::DEBUG);
    return inspectOutput;
}

std::string SkopeoDriver::manifestDigest(const boost::filesystem::path& manifestPath) const {
    if (!boost::filesystem::is_regular_file(manifestPath)) {
        auto message = boost::format("Path of manifest to digest %s does not lead to a regular file") % manifestPath;
        SARUS_THROW_ERROR(message.str());
    }

    printLog(boost::format("Manifest to digest: %s") % common::readFile(manifestPath), common::LogLevel::DEBUG);

    auto args = generateBaseArgs() + common::CLIArguments{"manifest-digest", manifestPath.string()};
    auto digestOutput = common::executeCommand(args.string());
    boost::algorithm::trim_right_if(digestOutput, boost::is_any_of("\n"));

    // The Skopeo debug messages are useful to be embedded in an exception message,
    // but we only want the digest string in the output.
    // Exclude the Skopeo debug lines and only return the last line of output
    if (common::Logger::getInstance().getLevel() == common::LogLevel::DEBUG) {
        digestOutput = digestOutput.substr(digestOutput.rfind('\n') + 1);
    }
    return digestOutput;
}

boost::filesystem::path SkopeoDriver::acquireAuthFile(const common::Config::Authentication& auth, const common::ImageReference& reference) {
    printLog("Acquiring authentication file", common::LogLevel::INFO);

    auto jsonPtrFormattedImageName = boost::regex_replace(reference.getFullName(), boost::regex("/"), "~1");
    auto jsonPointer = boost::format("/auths/%s/auth") % jsonPtrFormattedImageName;

    auto encodedCredentials = utility::base64Encode(auth.username + ":" + auth.password);

    auto authJSON = rapidjson::Document{};
    rapidjson::Pointer(jsonPointer.str().c_str()).Set(authJSON, encodedCredentials.c_str());

    common::createFoldersIfNecessary(authFileBasePath);
    authFilePath = common::makeUniquePathWithRandomSuffix(authFileBasePath / "auth.json");
    common::writeJSON(authJSON, authFilePath);
    boost::filesystem::permissions(authFilePath, boost::filesystem::perms::owner_read | boost::filesystem::perms::owner_write);

    printLog(boost::format("Successfully acquired authentication file %s") % authFilePath, common::LogLevel::INFO);
    return authFilePath;
}

common::CLIArguments SkopeoDriver::generateBaseArgs() const {
    auto args = common::CLIArguments{skopeoPath.string()};

    auto verbosity = getVerbosityOption();
    if (!verbosity.empty()) {
        args.push_back(verbosity);
    }

    args += getPolicyOption();
    args += getRegistriesDOption();

    return args;
}

std::string SkopeoDriver::getVerbosityOption() const {
    auto logLevel = common::Logger::getInstance().getLevel();
    if (logLevel == common::LogLevel::DEBUG) {
        return std::string{"--debug"};
    }
    return std::string{};
}

common::CLIArguments SkopeoDriver::getPolicyOption() const {
    auto homePath = boost::filesystem::path{common::getEnvironmentVariable("HOME")};
    auto userPolicyPath = homePath / ".config/containers/policy.json";
    auto systemPolicyPath = boost::filesystem::path("/etc/containers/policy.json");

    if (enforceCustomPolicy) {
        return common::CLIArguments{"--policy", customPolicyPath.string()};
    }
    else if (boost::filesystem::exists(userPolicyPath)
             || boost::filesystem::exists(systemPolicyPath)) {
        return common::CLIArguments{};
    }
    else if (!customPolicyPath.empty()) {
        return common::CLIArguments{"--policy", customPolicyPath.string()};
    }
    else {
        SARUS_THROW_ERROR("Failed to detect default containers policy files and "
                          "no fallback policy file defined in sarus.json. "
                          "Please contact your system administrator.\nDocumentation reference: "
                          "https://sarus.readthedocs.io/en/stable/config/configuration_reference.html"
                          "#containerspolicy-object-optional")
    }
}

common::CLIArguments SkopeoDriver::getRegistriesDOption() const {
    if (!customRegistriesDPath.empty()) {
        return common::CLIArguments{"--registries.d", customRegistriesDPath.string()};
    }
    return common::CLIArguments{};
}

std::string SkopeoDriver::getTransportString(const std::string& transport) const {
    if (transport == std::string{"docker"}) {
        return std::string{"docker://"};
    }
    else if (transport == std::string{"docker-archive"}) {
        return std::string{"docker-archive:"};
    }
    else if (transport == std::string{"sif"}) {
        return std::string{"sif:"};
    }
    else if (transport == std::string{"containers-storage"}) {
        return std::string{"containers-storage:"};
    }

    auto message = boost::format("Transport type not supported: %s") % transport;
    SARUS_THROW_ERROR(message.str());
}

void SkopeoDriver::printLog(const boost::format &message, common::LogLevel level,
                            std::ostream& outStream, std::ostream& errStream) const {
    printLog(message.str(), level, outStream, errStream);
}

void SkopeoDriver::printLog(const std::string& message, common::LogLevel level,
                            std::ostream& outStream, std::ostream& errStream) const {
    common::Logger::getInstance().log(message, sysname, level, outStream, errStream);
}

}} // namespace
