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

#include <boost/algorithm/string/join.hpp>

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

    printLog(boost::format("Elapsed time on copy operation    : %s [sec]") % elapsed, common::LogLevel::INFO);
    printLog(boost::format("Successfully created OCI image"), common::LogLevel::INFO);

    return ociImagePath;
}

rapidjson::Document SkopeoDriver::inspect(const std::string& sourceTransport, const std::string& sourceReference) const {
    auto baseArgs = generateBaseArgs();
    auto baseArgsVector = std::vector<std::string>{baseArgs.begin(), baseArgs.end()};
    auto baseArgsString = boost::algorithm::join(baseArgsVector, " ");
    auto inspectCommand = boost::format("%s inspect %s%s")
                                        % baseArgsString
                                        % getTransportString(sourceTransport)
                                        % sourceReference;
    auto inspectOutput = common::executeCommand(inspectCommand.str());
    return common::parseJSON(inspectOutput);
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
