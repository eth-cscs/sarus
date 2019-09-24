/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Utility.hpp"

#include <iostream>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>
#include <boost/format.hpp>
#include <rapidjson/istreamwrapper.h>

#include "common/Error.hpp"
#include "common/Utility.hpp"

namespace rj = rapidjson;

namespace sarus {
namespace hooks {
namespace common {
namespace utility {

std::tuple<boost::filesystem::path, pid_t> parseStateOfContainerFromStdin() {
    rj::Document state;
    try {
        rj::IStreamWrapper sw(std::cin);
        state.ParseStream(sw);
    }
    catch(const std::exception& e) {
        SARUS_RETHROW_ERROR(e, "Failed to parse container's state JSON from stdin.");
    }

    boost::filesystem::path bundleDir = state["bundle"].GetString();
    pid_t pidOfContainer = state["pid"].GetInt();

    return std::tuple<boost::filesystem::path, pid_t>{bundleDir, pidOfContainer};
}

std::unordered_map<std::string, std::string> parseEnvironmentVariablesFromOCIBundle(const boost::filesystem::path& bundleDir) {
    auto env = std::unordered_map<std::string, std::string>{};
    auto json = sarus::common::readJSON(bundleDir / "config.json");
    for(const auto& variable : json["process"]["env"].GetArray()) {
        std::string k, v;
        std::tie(k, v) = sarus::common::parseEnvironmentVariable(variable.GetString());
        env[k] = v;
    }
    return env;
}

static void enterNamespace(const boost::filesystem::path& namespaceFile) {
    // get namespace's fd   
    auto fd = open(namespaceFile.c_str(), O_RDONLY);
    if (fd == -1) {
        auto message = boost::format("Failed to open namespace file %s: %s") % namespaceFile % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    // enter namespace
    if (setns(fd, 0) != 0) {
        auto message = boost::format("Failed to enter namespace %s: %s") % namespaceFile % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}

void enterNamespacesOfProcess(pid_t pid) {
    {
        auto file = boost::format("/proc/%s/ns/mnt") % pid;
        enterNamespace(file.str());
    }
    {
        auto file = boost::format("/proc/%s/ns/pid") % pid;
        enterNamespace(file.str());
    }
}

static void replaceFdIfAvailable(   const std::unordered_map<std::string, std::string>& env,
                                    int oldfd,
                                    const std::string& newfdEnvVariable) {
    auto it = env.find(newfdEnvVariable);
    if(it == env.cend()) {
        return;
    }

    auto newfd = std::stoi(it->second);
    if(dup2(newfd, oldfd) == -1) {
        auto message = boost::format("Failed to use %s with 'dup(%d, %d)': %s")
            % newfdEnvVariable
            % oldfd
            % newfd
            % std::strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}

void useSarusStdoutStderrIfAvailable(const std::unordered_map<std::string, std::string>& env) {
    replaceFdIfAvailable(env, 1, "SARUS_STDOUT_FD");
    replaceFdIfAvailable(env, 2, "SARUS_STDERR_FD");
}

}}}} // namespace
