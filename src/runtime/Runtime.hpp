/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_runtime_Runtime_hpp
#define sarus_runtime_Runtime_hpp

#include <memory>

#include "common/Config.hpp"
#include "common/SecurityChecks.hpp"
#include "runtime/OCIBundleConfig.hpp"


namespace sarus {
namespace runtime {

class Runtime {
public:
    Runtime(std::shared_ptr<const common::Config>);
    void setupOCIBundle() const;
    void executeContainer() const;

private:
    void setupMountIsolation() const;
    void setupRamFilesystem() const;
    void mountImageIntoRootfs() const;
    void setupDevFilesystem() const;
    void copyEtcFilesIntoRootfs() const;
    void performCustomMounts() const;
    void remountRootfsWithNoSuid() const;
    int  countExtraFileDescriptors() const;

private:
    std::shared_ptr<const common::Config> config;
    boost::filesystem::path bundleDir;
    boost::filesystem::path rootfsDir;
    OCIBundleConfig bundleConfig;
    common::SecurityChecks securityChecks;
};

}
}

#endif
