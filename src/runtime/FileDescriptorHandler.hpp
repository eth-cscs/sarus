/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_runtime_FileDescriptorHandler_hpp
#define sarus_runtime_FileDescriptorHandler_hpp

#include "common/Config.hpp"
#include "common/SecurityChecks.hpp"


namespace sarus {
namespace runtime {

class FileDescriptorHandler {
public:
    FileDescriptorHandler(std::shared_ptr<common::Config>);
    void prepareFileDescriptorsToPreserve();
    std::string getExtraFileDescriptors() const {return extraFileDescriptors;};

private:
    std::shared_ptr<common::Config> config;
    std::string extraFileDescriptors;
};

}
}

#endif
