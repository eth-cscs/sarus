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

#include <boost/algorithm/string/trim.hpp>

#include "common/Error.hpp"
#include "common/Utility.hpp"


namespace sarus {
namespace image_manager {

SkopeoDriver::SkopeoDriver(std::shared_ptr<const common::Config> config)
    : skopeoPath{config->json["skopeoPath"].GetString()},
      tempDir{config->directories.temp}
{}

boost::filesystem::path SkopeoDriver::copyToOCIImage(const std::string& sourceTransport, const std::string& sourceReference) const {
    auto ociImagePath = common::makeUniquePathWithRandomSuffix(tempDir / "ociImage");
    printLog( boost::format("Creating temporary OCI image in: %s") % ociImagePath, common::LogLevel::DEBUG);

    auto args = generateBaseArgs();
    args += common::CLIArguments{"copy",
                                 getTransportString(sourceTransport) + sourceReference,
                                 "oci:"+ociImagePath.string()+":sarus-oci-image"};

    auto start = std::chrono::system_clock::now();
    auto status = common::forkExecWait(args);
    if(status != 0) {
        auto message = boost::format("%s exited with code %d") % args % status;
        printLog(message, common::LogLevel::INFO);
        exit(status);
    }
    auto end = std::chrono::system_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / double(1000);
    printLog(boost::format("Elapsed time on copy operation: %s [sec]") % elapsed, common::LogLevel::INFO);
    printLog(boost::format("Successfully created OCI image"), common::LogLevel::INFO);

    return ociImagePath;
}

rapidjson::Document SkopeoDriver::inspectRaw(const std::string& sourceTransport, const std::string& sourceReference) const {
    auto inspectOutput = std::string{};
    auto args = generateBaseArgs();
    args += common::CLIArguments{"inspect", "--raw",
                                 getTransportString(sourceTransport) + sourceReference};

    auto start = std::chrono::system_clock::now();
    try {
        inspectOutput = common::executeCommand(args.string());
    }
    catch(common::Error& e) {
        // Confirm skopeo failed because of image non existent or unauthorized access
        auto errorMessage = std::string(e.what());
        auto exitCodeString = std::string{"Process terminated with status 1"};
        auto manifestErrorString = std::string{"Error reading manifest"};

        /**
         * Registries often respond differently to the same incorrect requests, making it very hard to
         * consistently understand whether an image is not present in the registry or it's just private.
         * For example, Docker Hub responds both "denied" and "unauthorized", regardless if the image is private or non-existent;
         * Quay.io responds "unauthorized" regardless if the image is private or non-existent.
         * Additionally, the error strings might have different contents depending on the registry.
         */
        auto deniedAccessString = std::string{"denied:"};
        auto unauthorizedAccessString = std::string{"unauthorized:"};

        if(errorMessage.find(exitCodeString) != std::string::npos
           && errorMessage.find(manifestErrorString) != std::string::npos) {
            auto messageHeader = boost::format{"Failed to pull image '%s'"
                                               "\nError reading manifest from registry."} % sourceReference;
            printLog(messageHeader, common::LogLevel::GENERAL, std::cerr);

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

    // The Skopeo debug messages are useful to be embedded in an exception message,
    // but prevent the output from being converted to JSON.
    // Exclude the Skopeo debug lines and only return the JSON output
    if (common::Logger::getInstance().getLevel() == common::LogLevel::DEBUG) {
        inspectOutput = inspectOutput.substr(inspectOutput.find("{"));
    }
    return common::parseJSON(inspectOutput);
}

std::string SkopeoDriver::manifestDigest(const boost::filesystem::path& manifestPath) const {
    if (!boost::filesystem::is_regular_file(manifestPath)) {
        auto message = boost::format("Path of manifest to digest %s does not lead to a regular file") % manifestPath;
        SARUS_THROW_ERROR(message.str());
    }

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

common::CLIArguments SkopeoDriver::generateBaseArgs() const {
    auto args = common::CLIArguments{skopeoPath.string()};

    auto verbosity = getVerbosityOption();
    if (!verbosity.empty()) {
        args.push_back(verbosity);
    }

    return args;
}

std::string SkopeoDriver::getVerbosityOption() const {
    auto logLevel = common::Logger::getInstance().getLevel();
    if (logLevel == common::LogLevel::DEBUG) {
        return std::string{"--debug"};
    }
    return std::string{};
}

std::string SkopeoDriver::getTransportString(const std::string& transport) const {
    if (transport == std::string{"docker"}) {
        return std::string{"docker://"};
    }
    else if (transport == std::string{"docker-archive"}) {
        return std::string{"docker-archive:"};
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
