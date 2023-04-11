/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_runtime_FileDescriptorHandler_hpp
#define sarus_runtime_FileDescriptorHandler_hpp

#include <boost/optional.hpp>

#include "common/Config.hpp"

namespace sarus {
namespace runtime {

class FileDescriptorHandler {
public:
    FileDescriptorHandler(std::shared_ptr<common::Config>);
    void preservePMIFdIfAny();
    void passStdoutAndStderrToHooks();
    void applyChangesToFdsAndEnvVariablesAndBundleAnnotations();
    int getExtraFileDescriptors() const {return extraFileDescriptors;};

private:
    struct FileDescriptorInfo {
        std::string name;
        boost::optional<std::string> containerEnvVariable;
        boost::optional<std::string> ociAnnotation;
        bool forceDup;
    };

private:
    std::vector<int> getOpenFileDescriptors() const;
    int duplicateFdAndPreserveBoth(int fd, const FileDescriptorInfo& info);
    int moveFdToLowestAvailableValue(int fd, const FileDescriptorInfo& info);

private:
    std::shared_ptr<common::Config> config;
    std::unordered_map<int, FileDescriptorInfo> fileDescriptorsToPreserve;
    int extraFileDescriptors;
};

}
}

#endif
