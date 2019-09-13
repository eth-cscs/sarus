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
    , extraFileDescriptors{0}
{detectFileDescriptorsToPreserve();}

void FileDescriptorHandler::detectFileDescriptorsToPreserve(){
    auto& hostEnvironment = config->commandRun.hostEnvironment;
    if (hostEnvironment.count("PMI_FD") > 0) {
        auto fd = std::stoi(hostEnvironment.at("PMI_FD"));
        fileDescriptorsToPreserve[fd] = "PMI_FD";
    }
}

void FileDescriptorHandler::prepareFileDescriptorsToPreserve() {
    if (fileDescriptorsToPreserve.empty()) {
        return;
    }
    utility::logMessage(boost::format("Preparing file descriptors to preserve into the container"), common::logType::INFO);

    for(const auto& fd : getOpenFileDescriptors()) {
        // Skip stdio descriptors
        if(fd <= 2) {
            continue;
        }

        // Close unwanted file descriptors
        auto it = fileDescriptorsToPreserve.find(fd);
        if(it == fileDescriptorsToPreserve.cend()) {
            utility::logMessage(boost::format("Closing file descriptor %d") % fd, common::logType::DEBUG);
            close(fd);
            continue;
        }

        // Ensure file descriptor to preserve is on the lowest available value
        const auto& fdName = it->second;
        if(fd == extraFileDescriptors + 3) {
            utility::logMessage(boost::format("No need to duplicate %s file descriptor %d") % fdName % fd,
                                common::logType::DEBUG);
        }
        else {
            utility::logMessage(boost::format("Duplicating %s fd %d") % fdName % fd, common::logType::DEBUG);
            auto newFd = dup(fd);
            if (newFd == -1) {
                auto message = boost::format("Could not duplicate %s file descriptor. Dup error: %s") % fdName % strerror(errno);
                SARUS_THROW_ERROR(message.str());
            }
            close(fd);
            fileDescriptorsToPreserve[newFd] = fdName;
            fileDescriptorsToPreserve.erase(it);
        }
        ++extraFileDescriptors;
    }

    // Use-case-specific actions
    for (const auto& it : fileDescriptorsToPreserve) {
        if (it.second == "PMI_FD") {
            config->commandRun.hostEnvironment["PMI_FD"] = std::to_string(it.first);
        }
    }
}

std::vector<int> FileDescriptorHandler::getOpenFileDescriptors() const {
    // Retrieve current process file descriptors
    auto processFdDir = boost::format("/proc/%d/fd") % getpid();
    auto openFds = std::vector<boost::filesystem::path>();
    for (const auto& entry : boost::filesystem::directory_iterator(processFdDir.str())) {
        openFds.push_back(entry.path());
    }

    // Filter actually existing fds
    auto existingFds = std::vector<int>();
    for (const auto& entry : openFds){
        if (boost::filesystem::exists(entry)){
            auto fdIdx = std::stoi(entry.filename().string());
            existingFds.push_back(fdIdx);
        }
    }

    std::sort(existingFds.begin(), existingFds.end());
    return existingFds;
}

} // namespace
} // namespace
