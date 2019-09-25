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
{
    // preserve stdio file descriptors by default
    fileDescriptorsToPreserve[0] = {"stdin", {}, {}, false};
    fileDescriptorsToPreserve[1] = {"stdout", {}, {}, false};
    fileDescriptorsToPreserve[2] = {"stderr", {}, {}, false};
}

void FileDescriptorHandler::preservePMIFdIfAny() {
    auto& hostEnvironment = config->commandRun.hostEnvironment;
    auto it = hostEnvironment.find("PMI_FD");
    if (it != hostEnvironment.cend()) {
        auto fd = std::stoi(it->second);
        fileDescriptorsToPreserve[fd] = { "PMI", std::string{"PMI_FD"}, {}, false };
    }
}

void FileDescriptorHandler::passStdoutAndStderrToHooks() {
    // Note: force duplication of stdout and stderr file descriptors because runc
    // replaces them prior executing the hooks, i.e. the Sarus's stdout and stderr
    // wouldn't be accessible from the hooks if not duplicated.
    fileDescriptorsToPreserve[1] = { "stdout", {}, std::string{"SARUS_STDOUT_FD"}, true };
    fileDescriptorsToPreserve[2] = { "stderr", {}, std::string{"SARUS_STDERR_FD"}, true };
}

void FileDescriptorHandler::applyChangesToFdsAndEnvVariables() {
    utility::logMessage("Applying changes to file descriptors and environment variables", common::logType::INFO);

    // close unwanted file descriptors
    for(auto fd: getOpenFileDescriptors()) {
        auto it = fileDescriptorsToPreserve.find(fd);
        if(it == fileDescriptorsToPreserve.cend()) {
            utility::logMessage(boost::format("Closing file descriptor %d") % fd, common::logType::DEBUG);
            close(fd);
        }
    }

    // process file descriptors to preserve
    for(auto fd : getOpenFileDescriptors()) {
        const auto& fdInfo = fileDescriptorsToPreserve[fd];
        int newFd;

        // Ensure file descriptor to preserve is on the lowest available value
        bool isAtLowestAvailableValue = (fd <= 3 + extraFileDescriptors);
        if(isAtLowestAvailableValue && !fdInfo.forceDup) {
            utility::logMessage(boost::format("No need to duplicate %s file descriptor %d") % fdInfo.name % fd,
                                common::logType::DEBUG);
            newFd = fd;
        }
        else {
            utility::logMessage(boost::format("Duplicating %s fd %d") % fdInfo.name % fd, common::logType::DEBUG);
            newFd = dup(fd);
            if (newFd == -1) {
                auto message = boost::format("Could not duplicate %s file descriptor. Dup error: %s") % fdInfo.name % strerror(errno);
                SARUS_THROW_ERROR(message.str());
            }
            utility::logMessage(boost::format("New %s fd: %d") % fdInfo.name % newFd, common::logType::DEBUG);
            if(!fdInfo.forceDup) {
                close(fd);
            }
        }

        if(fd > 2 && fdInfo.forceDup) {
            ++extraFileDescriptors;
        }

        if(newFd > 2) {
            ++extraFileDescriptors;
        }

        if(fdInfo.containerEnvVariable) {
            utility::logMessage(boost::format("Setting container env variable %s=%d") % *fdInfo.containerEnvVariable % newFd, common::logType::DEBUG);
            config->commandRun.hostEnvironment[*fdInfo.containerEnvVariable] = std::to_string(newFd);
        }

        if(fdInfo.hookEnvVariable) {
            utility::logMessage(boost::format("Setting hooks env variable %s=%d") % *fdInfo.hookEnvVariable % newFd, common::logType::DEBUG);
            config->commandRun.hooksEnvironment[*fdInfo.hookEnvVariable] = std::to_string(newFd);
        }
    }

    utility::logMessage(boost::format("Total extra file descriptors: %d") % extraFileDescriptors, common::logType::DEBUG);
    utility::logMessage("Successfully applied changes to file descriptors and environment variables", common::logType::INFO);
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
