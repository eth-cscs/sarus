/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "FileDescriptorHandler.hpp"

#include <fcntl.h>
#include <sys/types.h>

#include <boost/format.hpp>

#include "runtime/Utility.hpp"
#include "common/Error.hpp"
#include "common/Utility.hpp"


namespace sarus {
namespace runtime {

FileDescriptorHandler::FileDescriptorHandler(std::shared_ptr<common::Config> config)
    : config{config}
    , hostEnvironment{config->commandRun.hostEnvironment}
    , extraFileDescriptors{0}
{detectFileDescriptorsToPreserve();}

void FileDescriptorHandler::detectFileDescriptorsToPreserve(){
    auto& hostEnvironment = config->commandRun.hostEnvironment;
    if (hostEnvironment.count("PMI_FD") > 0) {
        fileDescriptorsToPreserve["PMI_FD"] = std::stoi(hostEnvironment.at("PMI_FD"));
    }
}

void FileDescriptorHandler::prepareFileDescriptorsToPreserve() {
    if (fileDescriptorsToPreserve.empty()) {
        return;
    }

    closeUnwantedFileDescriptors();

    // Duplicate PMI_FD to lowest available fd. Set environment variable to lowest PMI_FD
    if (fileDescriptorsToPreserve.count("PMI_FD") > 0) {
        utility::logMessage(boost::format("Preparing to preserve PMI2 file descriptor"), common::logType::INFO);
        auto hostPMIFD = fileDescriptorsToPreserve.at("PMI_FD");
        auto newPMIFD = dup(hostPMIFD);
        utility::logMessage(boost::format("Host PMI FD: %s") % std::to_string(hostPMIFD), common::logType::DEBUG);
        utility::logMessage(boost::format("New PMI FD: %s") % std::to_string(newPMIFD), common::logType::DEBUG);
        if (newPMIFD == -1) {
            utility::logMessage(boost::format("PMI_FD dup error: %s") % strerror(errno), common::logType::DEBUG);
            utility::logMessage(boost::format("Could not duplicate PMI2 file descriptor. Attempting to preserve host "
                    "file descriptor inside the container"), common::logType::WARN);
            updateExtraFileDescriptors(hostPMIFD);
        }
        else if (newPMIFD < hostPMIFD) {
            config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(newPMIFD);
            updateExtraFileDescriptors(newPMIFD);
        }
        else {
            updateExtraFileDescriptors(hostPMIFD);
        }
    }
}

void FileDescriptorHandler::closeUnwantedFileDescriptors() {
    utility::logMessage(boost::format("Closing file descriptors not to preserve into the container"), common::logType::INFO);

    // Retrieve current process file descriptors
    auto processFD = boost::format("/proc/%d/fd") % getpid();
    auto openFDs = std::vector<boost::filesystem::path>();
    auto existingFDs = std::vector<boost::filesystem::path>();
    for (const auto& entry : boost::filesystem::directory_iterator(processFD.str())) {
        openFDs.push_back(entry.path());
    }

    // Filter actually existing fds
    for (const auto& entry : openFDs){
        if (boost::filesystem::exists(entry)){
            existingFDs.push_back(entry);
        }
    }

    // Keep open only fds to preserve
    for (const auto& entry : existingFDs) {
        auto fdIdx = std::stoi(entry.filename().string());

        // Skip stdio descriptors
        if (fdIdx < 3) {
            continue;
        }

        bool closeFD = true;
        for (const auto& toPreserve : fileDescriptorsToPreserve) {
            if (fdIdx == toPreserve.second) {
                closeFD = false;
            }
        }

        if (closeFD) {
            utility::logMessage(boost::format("Attempting to close file descriptor %d") % fdIdx, common::logType::DEBUG);
            if (close(fdIdx)) {
                utility::logMessage(boost::format("Could not close file descriptor %d ")
                % fdIdx, common::logType::WARN);
            }
        }
    }
}

void FileDescriptorHandler::updateExtraFileDescriptors(int fd) {
    auto newExtra = fd - 3 + 1; // Subtract number of stdio fds, add 1 because of 0-based indexing
    extraFileDescriptors = std::max(extraFileDescriptors, newExtra);
    utility::logMessage(boost::format("New number of extra file descriptors: %d")
        % extraFileDescriptors, common::logType::DEBUG);
}

} // namespace
} // namespace
