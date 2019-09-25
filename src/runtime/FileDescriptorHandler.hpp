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

#include <boost/optional.hpp>

#include "common/Config.hpp"
#include "common/SecurityChecks.hpp"


namespace sarus {
namespace runtime {

class FileDescriptorHandler {
public:
    FileDescriptorHandler(std::shared_ptr<common::Config>);
    void preservePMIFdIfAny();
    void passStdoutAndStderrToHooks();
    void applyChangesToFdsAndEnvVariables();
    int getExtraFileDescriptors() const {return extraFileDescriptors;};

private:
    void detectFileDescriptorsToPreserve();
    std::vector<int> getOpenFileDescriptors() const;

private:
    struct FileDescriptorInfo {
        std::string name;
        boost::optional<std::string> containerEnvVariable;
        boost::optional<std::string> hookEnvVariable;
        bool forceDup;
    };

private:
    std::shared_ptr<common::Config> config;
    std::unordered_map<int, FileDescriptorInfo> fileDescriptorsToPreserve;
    int extraFileDescriptors;
};

}
}

#endif
