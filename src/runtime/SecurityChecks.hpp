/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_runtime_SecurityChecks_hpp
#define sarus_runtime_SecurityChecks_hpp

#include <memory>

#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "common/Config.hpp"


namespace sarus {
namespace runtime {

class SecurityChecks {
public:
    SecurityChecks(std::shared_ptr<const common::Config>);
    void checkThatPathIsUntamperable(const boost::filesystem::path&) const;
    void checkThatBinariesInSarusJsonAreUntamperable() const;
    void checkThatOCIHooksAreUntamperable() const;
    void runSecurityChecks(const boost::filesystem::path& sarusInstallationPrefixDir) const;

private:
    void checkThatPathIsRootOwned(const boost::filesystem::path& path) const;
    void checkThatPathIsNotGroupWritableOrWorldWritable(const boost::filesystem::path& path) const;

private:
    std::shared_ptr<const common::Config> config;
};

}
}

#endif
