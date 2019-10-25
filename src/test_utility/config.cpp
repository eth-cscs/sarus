/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "config.hpp"

#include <memory>

#include <unistd.h>

#include "common/Utility.hpp"

namespace rj = rapidjson;
using namespace sarus;

namespace test_utility {
namespace config {

ConfigRAII::~ConfigRAII() {
    {
        auto dir = boost::filesystem::path{ config->json["prefixDir"].GetString() };
        boost::filesystem::remove_all(dir);
    }
    {
        auto dir = boost::filesystem::path{ config->json["OCIBundleDir"].GetString() };
        boost::filesystem::remove_all(dir);
    }
    {
        auto dir = boost::filesystem::path{ config->json["localRepositoryBaseDir"].GetString() };
        boost::filesystem::remove_all(dir);
    }
    boost::filesystem::remove_all(config->directories.repository);
}

static void populateJSON(rj::Document& document) {
    auto& allocator = document.GetAllocator();

    auto prefixDir = common::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("sarus-test-prefix-dir"));
    auto bundleDir = prefixDir / "var/OCIBundle";
    auto localRepositoryBaseDir = common::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("sarus-test-localRepositoryBaseDir"));

    document.AddMember( "securityChecks",
                        false,
                        allocator);
    document.AddMember( "OCIBundleDir",
                        rj::Value{bundleDir.c_str(), allocator},
                        allocator);
    document.AddMember( "rootfsFolder",
                        rj::Value{"rootfs", allocator},
                        allocator);
    document.AddMember( "prefixDir",
                        rj::Value{prefixDir.c_str(), allocator},
                        allocator);
    document.AddMember( "tempDir",
                        rj::Value{"/tmp", allocator},
                        allocator);
    document.AddMember( "localRepositoryBaseDir",
                        rj::Value{localRepositoryBaseDir.c_str(), allocator},
                        allocator);
    document.AddMember( "ramFilesystemType",
                        rj::Value{"ramfs"},
                        allocator);
    document.AddMember( "mksquashfsPath",
                        rj::Value{"/usr/bin/mksquashfs", allocator},
                        allocator);
    document.AddMember( "initPath",
                        rj::Value{"/usr/bin/init-program", allocator},
                        allocator);
    document.AddMember( "runcPath",
                        rj::Value{"/usr/bin/runc.amd64", allocator},
                        allocator);

    // siteMounts
    rj::Value siteMountsValue(rj::kArrayType);
    rj::Value mountValue(rj::kObjectType);
    mountValue.AddMember(   "type",
                            rj::Value{"bind", allocator},
                            allocator);
    mountValue.AddMember(   "source",
                            rj::Value{"/source", allocator},
                            allocator);
    mountValue.AddMember(   "destination",
                            rj::Value{"/destination", allocator},
                            allocator);
    rj::Value flagsValue(rj::kObjectType);
    flagsValue.AddMember(   "bind-propagation",
                            rj::Value{"slave", allocator},
                            allocator);
    mountValue.AddMember(   "flags",
                            flagsValue,
                            allocator);
    siteMountsValue.PushBack(mountValue, allocator);
    document.AddMember("siteMounts", siteMountsValue, allocator);

    // userMounts
    rj::Value userMountsValue(rj::kObjectType);
    rj::Value disallowWithPrefixValue(rj::kArrayType);
    disallowWithPrefixValue.PushBack("/etc", allocator);
    disallowWithPrefixValue.PushBack("/var", allocator);
    disallowWithPrefixValue.PushBack("/opt/sarus", allocator);
    userMountsValue.AddMember("notAllowedPrefixesOfPath", disallowWithPrefixValue, allocator);
    rj::Value disallowExactValue(rj::kArrayType);
    disallowExactValue.PushBack("/opt", allocator);
    userMountsValue.AddMember("notAllowedPaths", disallowExactValue, allocator);
    document.AddMember("userMounts", userMountsValue, allocator);

    // OCIHooks
    auto hook = rj::Value{ rj::kObjectType };
    hook.AddMember("path", rj::Value{"/bin/hook"}, allocator);
    hook.AddMember("env", rj::Value{rj::kArrayType}, allocator);
    auto hooks = rj::Value{ rj::kObjectType };
    hooks.AddMember("prestart", rj::Value{rj::kArrayType}, allocator);
    hooks["prestart"].GetArray().PushBack(hook, allocator);
    document.AddMember("OCIHooks", hooks, allocator);
}

ConfigRAII makeConfig() {
    auto raii = ConfigRAII{};
    raii.config = std::make_shared<common::Config>();

    populateJSON(raii.config->json);

    auto prefixDir = boost::filesystem::path{ raii.config->json["prefixDir"].GetString() };
    common::createFoldersIfNecessary(prefixDir / "etc");
    common::executeCommand("getent passwd >" + prefixDir.string() + "/etc/passwd");
    common::executeCommand("getent group >" + prefixDir.string() + "/etc/group");

    auto repository = common::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("./sarus-test-repository"));
    raii.config->directories.repository = repository;
    raii.config->directories.temp = raii.config->directories.repository / "temp";
    raii.config->directories.cache = raii.config->directories.repository / "cache";
    raii.config->directories.images = raii.config->directories.repository / "images";

    raii.config->commandRun.hostEnvironment = {{"key", "value"}};

    return raii;
}

}
}
