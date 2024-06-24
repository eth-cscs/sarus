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

#include "common/CLIArguments.hpp"
#include "common/LogLevel.hpp"
#include "common/UserIdentity.hpp"
#include "common/Mount.hpp"
#include "common/DeviceMount.hpp"

#include "common/Utility.hpp"

namespace sarus {
namespace hooks {
namespace mount {

class MountHook {
public:
    MountHook(const sarus::common::CLIArguments& args);
    void activate() const;

    // for testing purposes
    const std::vector<std::shared_ptr<sarus::common::Mount>> getBindMounts() const {return bindMounts;};

private:
    void parseConfigJSONOfBundle();
    void parseEnvironmentVariables();
    void parseCliArguments(const sarus::common::CLIArguments& args);
    std::string replaceStringWildcards(const std::string& input);
    std::string replaceFiProviderPathWildcard(const std::string& input);
    boost::filesystem::path findLibfabricLibdir() const;
    void performBindMounts() const;
    void performDeviceMounts() const;
    void log(const std::string& message, sarus::common::LogLevel level) const;
    void log(const boost::format& message, sarus::common::LogLevel level) const;

private:
    common::hook::ContainerState containerState;
    boost::filesystem::path rootfsDir;
    sarus::common::UserIdentity userIdentity;
    boost::filesystem::path ldconfigPath;
    boost::filesystem::path fiProviderPath;
    std::vector<std::shared_ptr<sarus::common::Mount>> bindMounts;
    std::vector<std::shared_ptr<sarus::common::DeviceMount>> deviceMounts;
};

}}} // namespace

#endif
