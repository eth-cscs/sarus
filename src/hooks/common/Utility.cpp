/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
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
#include "common/Logger.hpp"

namespace rj = rapidjson;

namespace sarus {
namespace hooks {
namespace common {
namespace utility {

static void replaceFd(int oldfd, int newfd) {
    if(dup2(newfd, oldfd) == -1) {
        auto message = boost::format("Failed to replace fd with 'dup(%d, %d)': %s")
            % oldfd
            % newfd
            % std::strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}

void applyLoggingConfigIfAvailable(const rapidjson::Document& json) {
    if(!json.HasMember("annotations")) {
        return;
    }

    if(json["annotations"].HasMember("com.hooks.logging.level")) {
        auto str = json["annotations"]["com.hooks.logging.level"].GetString();
        auto level = static_cast<sarus::common::LogLevel>(std::stoi(str));
        sarus::common::Logger::getInstance().setLevel(level);
    }

    if(json["annotations"].HasMember("com.hooks.logging.stdoutfd")) {
        replaceFd(1, std::stoi(json["annotations"]["com.hooks.logging.stdoutfd"].GetString()));
    }

    if(json["annotations"].HasMember("com.hooks.logging.stderrfd")) {
        replaceFd(2, std::stoi(json["annotations"]["com.hooks.logging.stderrfd"].GetString()));
    }
}

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

}}}} // namespace
