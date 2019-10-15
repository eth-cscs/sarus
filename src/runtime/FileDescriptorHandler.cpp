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
    utility::logMessage("Applying changes to file descriptors and environment variables", common::LogLevel::INFO);

    // close unwanted file descriptors
    for(auto fd : getOpenFileDescriptors()) {
        bool toBePreserved = fileDescriptorsToPreserve.find(fd) != fileDescriptorsToPreserve.cend();
        if(!toBePreserved) {
            utility::logMessage(boost::format("Closing file descriptor %d") % fd, common::LogLevel::DEBUG);
            close(fd);
        }
    }

    // process remaining (wanted) file descriptors
    for(auto fd : getOpenFileDescriptors()) {
        const auto& fdInfo = fileDescriptorsToPreserve[fd];
        int newFd;

        if(fdInfo.forceDup) {
            newFd = duplicateFd(fd, fdInfo);
        }
        else {
            newFd = moveFdToLowestAvailableValue(fd, fdInfo);
        }

        if(fdInfo.containerEnvVariable) {
            utility::logMessage(boost::format("Setting container env variable %s=%d") % *fdInfo.containerEnvVariable % newFd, common::LogLevel::DEBUG);
            config->commandRun.hostEnvironment[*fdInfo.containerEnvVariable] = std::to_string(newFd);
        }

        if(fdInfo.hookEnvVariable) {
            utility::logMessage(boost::format("Setting hooks env variable %s=%d") % *fdInfo.hookEnvVariable % newFd, common::LogLevel::DEBUG);
            config->commandRun.hooksEnvironment[*fdInfo.hookEnvVariable] = std::to_string(newFd);
        }
    }

    utility::logMessage(boost::format("Total extra file descriptors: %d") % extraFileDescriptors, common::LogLevel::DEBUG);
    utility::logMessage("Successfully applied changes to file descriptors and environment variables", common::LogLevel::INFO);
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

int FileDescriptorHandler::duplicateFd(int fd, const FileDescriptorHandler::FileDescriptorInfo& fdInfo) {
    auto newFd = dup(fd);
    if(newFd == -1) {
        auto message = boost::format("Could not duplicate %s file descriptor. Dup error: %s") % fdInfo.name % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    if(newFd < fd) {
        auto message = boost::format("Internal error: attempted to make a forced duplication of fd %d, but dup()"
                                     " created a lower fd %d. This means that forcing the duplication might"
                                     " create gabs in the resulting sequence of open fds.") % fd % newFd;
        SARUS_THROW_ERROR(message.str());
    }
    if(fd > 2) {
        ++extraFileDescriptors;
    }
    if(newFd > 2) {
        ++extraFileDescriptors;
    }
    return newFd;
}

int FileDescriptorHandler::moveFdToLowestAvailableValue(int fd, const FileDescriptorHandler::FileDescriptorInfo& fdInfo) {
    int newFd;
    bool isAtLowestAvailableValue = (fd <= 3 + extraFileDescriptors);
    if(isAtLowestAvailableValue) {
        utility::logMessage(boost::format("No need to move %s file descriptor %d (already at lowest available value)")
                            % fdInfo.name % fd,
                            common::LogLevel::DEBUG);
        newFd = fd;
    }
    else {
        newFd = dup(fd);
        if (newFd == -1) {
            auto message = boost::format("Could not duplicate %s file descriptor. Dup error: %s") % fdInfo.name % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }
        close(fd);
    }
    if(newFd > 2) {
        ++extraFileDescriptors;
    }
    return newFd;
}

} // namespace
} // namespace
