/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_mpi_MountHook_hpp
#define sarus_hooks_mpi_MountHook_hpp

#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include "libsarus/CLIArguments.hpp"
#include "libsarus/LogLevel.hpp"
#include "libsarus/UserIdentity.hpp"
#include "libsarus/Mount.hpp"
#include "libsarus/DeviceMount.hpp"

#include "libsarus/Utility.hpp"

namespace sarus {
namespace hooks {
namespace mount {

class MountHook {
public:
    MountHook(const libsarus::CLIArguments& args);
    void activate() const;

    // for testing purposes
    const std::vector<std::shared_ptr<libsarus::Mount>> getBindMounts() const {return bindMounts;};

private:
    void parseConfigJSONOfBundle();
    void parseEnvironmentVariables();
    void parseCliArguments(const libsarus::CLIArguments& args);
    std::string replaceStringWildcards(const std::string& input);
    std::string replaceFiProviderPathWildcard(const std::string& input);
    boost::filesystem::path findLibfabricLibdir() const;
    void performBindMounts() const;
    void performDeviceMounts() const;
    void log(const std::string& message, libsarus::LogLevel level) const;
    void log(const boost::format& message, libsarus::LogLevel level) const;

private:
    libsarus::hook::ContainerState containerState;
    boost::filesystem::path rootfsDir;
    libsarus::UserIdentity userIdentity;
    boost::filesystem::path ldconfigPath;
    boost::filesystem::path fiProviderPath;
    std::vector<std::shared_ptr<libsarus::Mount>> bindMounts;
    std::vector<std::shared_ptr<libsarus::DeviceMount>> deviceMounts;
};

}}} // namespace

#endif
