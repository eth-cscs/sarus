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
    , extraFileDescriptors{"0"}
{}

void FileDescriptorHandler::prepareFileDescriptorsToPreserve() {
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

    // Close close-on-exec fds
    for (unsigned int fdIdx = 0; fdIdx < existingFDs.size(); ++fdIdx) {
        if (fcntl(fdIdx,F_GETFD) == 1) {
            if (close(fdIdx)) {
                utility::logMessage(boost::format("Could not pre-emptively close close-on-exec file descriptor %d ")
                % fdIdx, common::logType::WARN);
            }
        }
    }

    // Duplicate PMI_FD (if present) to lowest available fd. Set environment variable to lowest PMI_FD
    auto& hostEnvironment = config->commandRun.hostEnvironment;
    if (hostEnvironment.count("PMI_FD") > 0) {
        auto hostPMIFD = std::stoi(hostEnvironment.at("PMI_FD"));
        auto newPMIFD = dup(hostPMIFD);
        utility::logMessage(boost::format("Host PMI FD: %s") % std::to_string(hostPMIFD), common::logType::DEBUG);
        utility::logMessage(boost::format("New PMI FD: %s") % std::to_string(newPMIFD), common::logType::DEBUG);
        if (newPMIFD == -1) {
            utility::logMessage(boost::format("PMI_FD dup error: %s") % strerror(errno), common::logType::DEBUG);
            utility::logMessage(boost::format("Could not duplicate PMI2 file descriptor. Attempting to preserve host "
                    "file descriptor inside the container"), common::logType::WARN);
            extraFileDescriptors = std::to_string(hostPMIFD - 3 + 1); // subtracting number of stdio fds, adding 1 because of 0-based indexing
        }
        else if (newPMIFD < hostPMIFD) {
            hostEnvironment["PMI_FD"] = std::to_string(newPMIFD);
            extraFileDescriptors = std::to_string(newPMIFD - 3 + 1); // subtracting number of stdio fds, adding 1 because of 0-based indexing
        }
        else {
            extraFileDescriptors = std::to_string(hostPMIFD - 3 + 1); // subtracting number of stdio fds, adding 1 because of 0-based indexing
        }
    }
}

} // namespace
} // namespace
