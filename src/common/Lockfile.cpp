/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Lockfile.hpp"

#include <chrono>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/Logger.hpp"


namespace sarus {
namespace common {

Lockfile::Lockfile()
    : logger(&common::Logger::getInstance())
{}

Lockfile::Lockfile(const boost::filesystem::path& file, unsigned int timeoutMs)
    : logger(&common::Logger::getInstance())
    , lockfile{convertToLockfile(file)}
{
    auto message = boost::format("acquiring lock on file %s") % file;
    logger->log(message.str(), loggerSubsystemName, common::LogLevel::DEBUG);

    unsigned int elapsedTimeMs = 0;
    while(!createLockfileAtomically()) {
        if(timeoutMs != noTimeout && elapsedTimeMs >= timeoutMs) {
            message = boost::format("failed to acquire lock on file %s (expired timeout of %d milliseconds)") % *lockfile % timeoutMs;
            SARUS_THROW_ERROR(message.str());
        }
        const int backoffTimeMs = 100;
        std::this_thread::sleep_for(std::chrono::milliseconds(backoffTimeMs));
        elapsedTimeMs += backoffTimeMs;
    }

    logger->log("successfully acquired lock", loggerSubsystemName, common::LogLevel::DEBUG);
}

Lockfile::Lockfile(Lockfile&& rhs)
    : logger{rhs.logger}
    , lockfile{rhs.lockfile}
{
    logger->log("move constructing lock", loggerSubsystemName, common::LogLevel::DEBUG);
    rhs.lockfile.reset();
    logger->log("successfully move constructed lock", loggerSubsystemName, common::LogLevel::DEBUG);
}

Lockfile& Lockfile::operator=(Lockfile&& rhs) {
    logger->log("move assigning lock", loggerSubsystemName, common::LogLevel::DEBUG);
    if(lockfile) {
        boost::filesystem::remove(*lockfile);
    }
    lockfile = rhs.lockfile;
    rhs.lockfile.reset();
    logger->log("successfully move assigned lock", loggerSubsystemName, common::LogLevel::DEBUG);
    return *this;
}

Lockfile::~Lockfile() {
    logger->log("destroying lockfile", loggerSubsystemName, common::LogLevel::DEBUG);
    if(lockfile) {
        auto message = boost::format("removing lockfile %s") % *lockfile;
        logger->log(message.str(), loggerSubsystemName, common::LogLevel::DEBUG);
        boost::filesystem::remove(*lockfile);
    }
    logger->log("successfully destroyed lockfile", loggerSubsystemName, common::LogLevel::DEBUG);
}

boost::filesystem::path Lockfile::convertToLockfile(const boost::filesystem::path& file) const {
    auto lockfileLeaf = file.leaf().string() + ".lock";
    auto lockfile = file;
    lockfile.remove_leaf();
    lockfile /= lockfileLeaf;

    auto message = boost::format("converted filename %s to lockfile %s") % file % lockfile;
    logger->log(message.str(), loggerSubsystemName, common::LogLevel::DEBUG);

    return lockfile;
}

bool Lockfile::createLockfileAtomically() const {
    auto message = boost::format("creating lockfile %s") % *lockfile;
    logger->log(message.str(), loggerSubsystemName, common::LogLevel::DEBUG);

    auto fd = open(lockfile->string().c_str(), O_CREAT | O_EXCL, O_RDONLY);
    if(fd == -1) {
        message = boost::format("failed to create lockfile %s") % *lockfile;
        logger->log(message.str(), loggerSubsystemName, common::LogLevel::DEBUG);
        return false;
    }

    logger->log("successfully created lockfile", loggerSubsystemName, common::LogLevel::DEBUG);

    if(close(fd) != 0) {
        message = boost::format("failed to close file descriptor of lockfile %s") % *lockfile;
        SARUS_THROW_ERROR(message.str());
    }
    return true;
}

} // namespace
} // namespace
