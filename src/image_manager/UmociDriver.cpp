/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "image_manager/UmociDriver.hpp"

#include <chrono>

#include "libsarus/Error.hpp"
#include "libsarus/Utility.hpp"


namespace sarus {
namespace image_manager {

UmociDriver::UmociDriver(std::shared_ptr<const common::Config> config)
    : umociPath{config->json["umociPath"].GetString()}
{
    if (!boost::filesystem::is_regular_file(umociPath)) {
        auto message = boost::format("The path to the Umoci executable '%s' configured in sarus.json does not "
                                     "lead to a regular file. "
                                     "Please contact your system administrator.") % umociPath;
        SARUS_THROW_ERROR(message.str());
    }
}

void UmociDriver::unpack(const boost::filesystem::path& imagePath, const boost::filesystem::path& unpackPath) const {
    printLog( boost::format("Unpacking OCI image from %s into %s") % imagePath % unpackPath, libsarus::LogLevel::DEBUG);

    auto args = generateBaseArgs();
    args += libsarus::CLIArguments{"raw", "unpack", "--rootless",
                                 "--image", imagePath.string()+":sarus-oci-image",
                                 unpackPath.string()};

    auto start = std::chrono::system_clock::now();
    auto status = libsarus::process::forkExecWait(args);
    if(status != 0) {
        auto message = boost::format("Failed to unpack OCI image %s") % imagePath;
        SARUS_THROW_ERROR(message.str());
    }
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / double(1000);

    printLog(boost::format("Elapsed time on unpacking    : %s [sec]") % elapsed, libsarus::LogLevel::INFO);
}

libsarus::CLIArguments UmociDriver::generateBaseArgs() const {
    auto args = libsarus::CLIArguments{umociPath.string()};

    auto verbosity = getVerbosityOption();
    if (!verbosity.empty()) {
        args.push_back(verbosity);
    }

    return args;
}

std::string UmociDriver::getVerbosityOption() const {
    auto logLevel = libsarus::Logger::getInstance().getLevel();
    if (logLevel == libsarus::LogLevel::DEBUG) {
        return std::string{"--log=debug"};
    }
    else if (logLevel == libsarus::LogLevel::INFO) {
        return std::string{"--log=info"};
    }
    return std::string{"--log=error"};
}

void UmociDriver::printLog(const boost::format &message, libsarus::LogLevel level,
                            std::ostream& outStream, std::ostream& errStream) const {
    printLog(message.str(), level, outStream, errStream);
}

void UmociDriver::printLog(const std::string& message, libsarus::LogLevel level,
                            std::ostream& outStream, std::ostream& errStream) const {
    libsarus::Logger::getInstance().log(message, sysname, level, outStream, errStream);
}

}} // namespace
