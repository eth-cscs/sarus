/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Flock.hpp"

#include <chrono>
#include <thread>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <boost/format.hpp>

#include "libsarus/Error.hpp"
#include "libsarus/Logger.hpp"

namespace libsarus {

const milliseconds Flock::noTimeout = milliseconds{std::numeric_limits<unsigned long long>::max()};

Flock::Flock()
    : logger{&libsarus::Logger::getInstance()}
    , lockType{Type::readLock}
    , timeoutTime{milliseconds{noTimeout}}
    , warningTime{milliseconds{1000}}
{}

Flock::Flock(const boost::filesystem::path& file, const Type type, const milliseconds& timeoutMs, const milliseconds& warningMs)
    : logger{&libsarus::Logger::getInstance()}
    , lockfile{file}
    , lockType{type}
    , timeoutTime{timeoutMs}
    , warningTime{warningMs}
{
    auto message = boost::format("Initializing lock on file %s") % file;
    logger->log(message.str(), loggerSubsystemName, libsarus::LogLevel::DEBUG);
    timedLockAcquisition();
    logger->log("Successfully initialized lock", loggerSubsystemName, libsarus::LogLevel::DEBUG);
}

Flock::Flock(Flock&& rhs)
    : logger{std::move(rhs.logger)}
    , lockfile{std::move(rhs.lockfile)}
    , lockType{std::move(rhs.lockType)}
    , fileFd{std::move(rhs.fileFd)}
    , timeoutTime{std::move(rhs.timeoutTime)}
    , warningTime{std::move(rhs.warningTime)}
{
    auto message = boost::format("move constructing lock for %s") % *rhs.lockfile;
    logger->log(message, loggerSubsystemName, libsarus::LogLevel::DEBUG);
    logger->log("successfully move constructed lock", loggerSubsystemName, libsarus::LogLevel::DEBUG);
}

Flock& Flock::operator=(Flock&& rhs) {
    auto message = boost::format("move assigning lock for %s") % *rhs.lockfile;
    logger->log(message, loggerSubsystemName, libsarus::LogLevel::DEBUG);
    // Release the lock from the current file before acquiring the one from the rhs
    // otherwise this process would be silently holding both locks
    this->release();
    lockType = std::move(rhs.lockType);
    lockfile = std::move(rhs.lockfile);
    fileFd = std::move(rhs.fileFd);
    timeoutTime = std::move(rhs.timeoutTime);
    warningTime = std::move(rhs.warningTime);
    logger->log("successfully move assigned lock", loggerSubsystemName, libsarus::LogLevel::DEBUG);
    return *this;
}

Flock::~Flock() {
    logger->log("destroying lockfile object", loggerSubsystemName, libsarus::LogLevel::DEBUG);
    release();
    logger->log("successfully destroyed lockfile object", loggerSubsystemName, libsarus::LogLevel::DEBUG);
}

void Flock::convertToType(const Type type) {
    if (type != lockType) {
        lockType = type;
        timedLockAcquisition();
    }
}

void Flock::timedLockAcquisition() {
    milliseconds elapsedTime{0};
    milliseconds backoffTime{100};
    while(!acquireLockAtomically()) {
        if(timeoutTime != milliseconds{noTimeout} && elapsedTime >= timeoutTime) {
            auto message = boost::format("Failed to acquire lock on file %s (expired timeout of %d milliseconds)") % *lockfile % timeoutTime.count();
            SARUS_THROW_ERROR(message.str());
        }
        std::this_thread::sleep_for(backoffTime);
        elapsedTime += backoffTime;
        if(elapsedTime.count() % warningTime.count() == 0) {
            auto message = boost::format("Still attempting to acquire lock on file %s after %d ms (will timeout after %d milliseconds)...")
                                   % *lockfile % elapsedTime.count() % timeoutTime.count();
            logger->log(message.str(), loggerSubsystemName, libsarus::LogLevel::WARN);
        }
    }
}

static std::pair<int, int> getFlockFlags(const Flock::Type &lockType) {
    if (lockType == Flock::Type::readLock) {
        return std::make_pair(O_RDONLY, LOCK_SH);
    }
    if (lockType == Flock::Type::writeLock) {
        return std::make_pair(O_RDWR, LOCK_EX);
    }
    auto message = boost::format("unknown lock type specified: %d") % lockType;
    SARUS_THROW_ERROR(message.str());
    return std::make_pair(-1, 0);
}

bool Flock::acquireLockAtomically() {
    int openMode;
    int flockOperation;
    std::tie(openMode, flockOperation) = getFlockFlags(lockType);

    auto message = boost::format("Attempting to acquire %s lock on file %s")
                   % (lockType==Type::readLock ? "read" : "write") % *lockfile;
    logger->log(message.str(), loggerSubsystemName, libsarus::LogLevel::DEBUG);

    if (fileFd < 0) {
        auto fd = open(lockfile->string().c_str(), 0, openMode);
        if(fd == -1) {
            message = boost::format("failed to open %s for locking") % *lockfile;
            logger->log(message.str(), loggerSubsystemName, libsarus::LogLevel::DEBUG);
            return false;
        }
        fileFd = fd;
    }

    if (flock(fileFd, flockOperation | LOCK_NB) == -1) {
        message = boost::format("failed to flock() on %s (fd %d): %s") % *lockfile % fileFd % strerror(errno);
        logger->log(message.str(), loggerSubsystemName, libsarus::LogLevel::DEBUG);
        return false;
    }

    logger->log("successfully acquired lock", loggerSubsystemName, libsarus::LogLevel::DEBUG);
    return true;
}

void Flock::release() {
    if (fileFd >= 0) {
        if (flock(fileFd, LOCK_UN | LOCK_NB) == -1) {
            auto message = boost::format("failed to release lock on %s (fd %d): %s") % *lockfile % fileFd % strerror(errno);
            // FIXME: this should be a warning, but currently the lock handover during atomic updates of the local
            // repo metadata file is not completely clean, so it triggers this message about the temporary file,
            // even if there is nothing wrong and the operation completes successfully.
            // Temporarily demoting this message to INFO.
            logger->log(message.str(), loggerSubsystemName, libsarus::LogLevel::INFO);
        }
        if(close(fileFd) != 0) {
            auto message = boost::format("failed to close file descriptor %d of file %s") % fileFd % *lockfile;
            // FIXME: this should be a warning, but currently the lock handover during atomic updates of the local
            // repo metadata file is not completely clean, so it triggers this message about the temporary file,
            // even if there is nothing wrong and the operation completes successfully.
            // Temporarily demoting this message to INFO.
            logger->log(message.str(), loggerSubsystemName, libsarus::LogLevel::INFO);
        }
    }
}

}
