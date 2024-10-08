/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
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
#include "libsarus/Error.hpp"
#include "libsarus/Utility.hpp"


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

void FileDescriptorHandler::applyChangesToFdsAndEnvVariablesAndBundleAnnotations() {
    utility::logMessage("Applying changes to file descriptors, container's environment variables and bundle's annotations",
                        libsarus::LogLevel::INFO);

    // close unwanted file descriptors
    for(auto fd : getOpenFileDescriptors()) {
        bool toBePreserved = fileDescriptorsToPreserve.find(fd) != fileDescriptorsToPreserve.cend();
        if(!toBePreserved) {
            utility::logMessage(boost::format("Closing file descriptor %d") % fd, libsarus::LogLevel::DEBUG);
            close(fd);
        }
    }

    // process remaining (wanted) file descriptors
    for(auto fd : getOpenFileDescriptors()) {
        const auto& fdInfo = fileDescriptorsToPreserve[fd];
        int newFd;

        if(fdInfo.forceDup) {
            newFd = duplicateFdAndPreserveBoth(fd, fdInfo);
        }
        else {
            newFd = moveFdToLowestAvailableValue(fd, fdInfo);
        }

        if(fdInfo.containerEnvVariable) {
            utility::logMessage(boost::format("Setting container env variable %s=%d") % *fdInfo.containerEnvVariable % newFd, libsarus::LogLevel::DEBUG);
            config->commandRun.hostEnvironment[*fdInfo.containerEnvVariable] = std::to_string(newFd);
        }

        if(fdInfo.ociAnnotation) {
            // CLI should always have precedence when setting values: use emplace() to add the annotation
            // only if the key is not already present (i.e. previously added by CommandRun)
            utility::logMessage(boost::format("Attempting to set OCI annotation %s=%d") % *fdInfo.ociAnnotation % newFd, libsarus::LogLevel::DEBUG);
            config->commandRun.ociAnnotations.emplace(*fdInfo.ociAnnotation, std::to_string(newFd));
        }
    }

    utility::logMessage(boost::format("Total extra file descriptors: %d") % extraFileDescriptors, libsarus::LogLevel::DEBUG);
    utility::logMessage("Successfully applied changes to file descriptors, container's environment variables and bundle's annotations",
                        libsarus::LogLevel::INFO);
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

int FileDescriptorHandler::duplicateFdAndPreserveBoth(int fd, const FileDescriptorHandler::FileDescriptorInfo& fdInfo) {
    auto newFd = dup(fd);
    if(newFd == -1) {
        auto message = boost::format("Could not duplicate %s file descriptor. Error on dup(%d): %s")
                       % fdInfo.name % fd % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    if(newFd < fd) {
        auto message = boost::format("Internal error: attempted to make a forced duplication of fd %1%, but dup() created"
                                     " a lower fd %2% (<%1%). This means that the forced duplication might create gaps in the"
                                     " resulting sequence of open fds and some OCI runtimes, e.g. runc, do not expect such gaps.")
                        % fd % newFd;
        SARUS_THROW_ERROR(message.str());
    }
    if(fd > 2) {
        ++extraFileDescriptors;
    }
    if(newFd > 2) {
        ++extraFileDescriptors;
    }

    auto message = boost::format("Duplicated %s file descriptor (%d => %d). Preserving both file descriptors.") % fdInfo.name % fd % newFd;
    utility::logMessage(message.str(), libsarus::LogLevel::DEBUG);

    return newFd;
}

int FileDescriptorHandler::moveFdToLowestAvailableValue(int fd, const FileDescriptorHandler::FileDescriptorInfo& fdInfo) {
    int newFd;
    bool isAtLowestAvailableValue = (fd <= 3 + extraFileDescriptors);
    if(isAtLowestAvailableValue) {
        utility::logMessage(boost::format("No need to move %s file descriptor %d (already at lowest available value)")
                            % fdInfo.name % fd,
                            libsarus::LogLevel::DEBUG);
        newFd = fd;
    }
    else {
        newFd = dup(fd);
        if (newFd == -1) {
            auto message = boost::format("Could not move %s file descriptor. Error on dup(%s): %s")
                           % fdInfo.name % fd % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }
        close(fd);

        utility::logMessage(boost::format("Moved %1% file descriptor (%2% => %3%). Original fd %2% was closed.")
                            % fdInfo.name % fd % newFd,
                            libsarus::LogLevel::DEBUG);
    }
    if(newFd > 2) {
        ++extraFileDescriptors;
    }

    return newFd;
}

} // namespace
} // namespace
