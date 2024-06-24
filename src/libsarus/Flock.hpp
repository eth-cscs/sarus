/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_Flock_hpp
#define libsarus_Flock_hpp

#include <chrono>
#include <limits>

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

namespace libsarus {

using milliseconds = std::chrono::milliseconds;

class Logger;

/**
 * This class provides controlled access to a shared resource on the filesystem.
 * By wrapping usage of the flock(2) system call, this class supports both
 * shared access (a.k.a read lock) and exclusive access (a.k.a write lock) to
 * the resource. Obviously, only one type of access is active at any one time,
 * but the class allows to change between lock types.
 * The constructor attempts to acquire access to the shared resource by
 * calling flock(2) with the given operation type. If an incompatible lock type
 * already exists on the resource, the constructor busy waits until it is able
 * to acquire the lock or a timeout is reached.
 * The destructor and move operations release the lock on the shared resource.
 * Since the implementation relies on flock(2), this class only creates advisory
 * locks (see the man page for further details).
 */
class Flock {
public:
    static const milliseconds noTimeout;
    enum Type {readLock, writeLock};

public:
    Flock();
    Flock(const boost::filesystem::path& file, const Type type=readLock,
          const milliseconds& timeoutTime=noTimeout, const milliseconds& warningTime=milliseconds{1000});
    Flock(const Flock&) = delete;
    Flock(Flock&&);
    ~Flock();

    void convertToType(const Type type);

    Flock& operator=(const Flock&) = delete;
    Flock& operator=(Flock&&);

private:
    void timedLockAcquisition();
    bool acquireLockAtomically();
    void release();

private:
    libsarus::Logger* logger;
    std::string loggerSubsystemName = "Flock";
    boost::optional<boost::filesystem::path> lockfile;
    Type lockType;
    int fileFd = -1;
    milliseconds timeoutTime;
    milliseconds warningTime;
};

}

#endif
