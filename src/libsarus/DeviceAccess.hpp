/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_DeviceAccess_hpp
#define libsarus_DeviceAccess_hpp

#include <string>

#include "libsarus/Logger.hpp"

namespace libsarus {

class DeviceAccess {
public:
    DeviceAccess(const std::string& input);
    std::string string() const;
    bool isReadAllowed() const {return read;};
    bool isWriteAllowed() const {return write;};
    bool isMknodAllowed() const {return mknod;};

private:
    void parseInput(const std::string& input);
    void allowRead() {read = true;};
    void allowWrite() {write = true;};
    void allowMknod() {mknod = true;};
    void logMessage(const boost::format&, libsarus::LogLevel,
                    std::ostream& out=std::cout, std::ostream& err=std::cerr) const;
    void logMessage(const std::string&, libsarus::LogLevel,
                    std::ostream& out=std::cout, std::ostream& err=std::cerr) const;

private:
    bool read = false;
    bool write = false;
    bool mknod = false;
};

}

#endif
