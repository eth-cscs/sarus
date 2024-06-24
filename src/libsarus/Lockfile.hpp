/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_Lockfile_hpp
#define libsarus_Lockfile_hpp

#include <limits>

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

namespace libsarus {

class Logger;

/**
 * This class provides exclusive access to a shared resource on the filesystem.
 * The constructor attempts to acquire exclusive access to the shared resource by
 * atomically creating a lock file on the filesystem. If a lock file already exists,
 * i.e. the resource was already acquired by somebody else, the constructor
 * busy waits until the lock file is removed.
 * The destructor releases exclusive access to the shared resource by removing
 * the lock file from the filesystem.
 */
class Lockfile {
public:
    static constexpr unsigned int noTimeout = std::numeric_limits<unsigned int>::max();

public:
    Lockfile();
    Lockfile(const boost::filesystem::path& file, unsigned int timeoutMs=noTimeout, unsigned int warningMs=1000);
    Lockfile(const Lockfile&) = delete;
    Lockfile(Lockfile&&);
    ~Lockfile();

    Lockfile& operator=(const Lockfile&) = delete;
    Lockfile& operator=(Lockfile&&);

private:
    boost::filesystem::path convertToLockfile(const boost::filesystem::path& file) const;
    bool createLockfileAtomically() const;

private:
    libsarus::Logger* logger;
    std::string loggerSubsystemName = "Lockfile";
    boost::optional<boost::filesystem::path> lockfile;
};

}

#endif
