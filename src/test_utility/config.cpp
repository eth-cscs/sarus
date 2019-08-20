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
        auto dir = boost::filesystem::path{ config->json["dirOfFilesToCopyInContainerEtc"].GetString() };
        boost::filesystem::remove_all(dir);
    }
    {
        auto dir = boost::filesystem::path{ config->json["localRepositoryBaseDir"].GetString() };
        boost::filesystem::remove_all(dir);
    }
    boost::filesystem::remove_all(config->directories.repository);
}

static void populateJSON(rapidjson::Document& document) {
    auto& allocator = document.GetAllocator();

    auto prefixDir = common::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("./sarus-test-prefix-dir"));
    auto bundleDir = prefixDir / "var/OCIBundle";
    auto dirOfFilesToCopyInContainerEtc = common::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("./sarus-test-dirOfFilesToCopyInContainerEtc"));
    auto localRepositoryBaseDir = common::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("./sarus-test-localRepositoryBaseDir"));

    document.AddMember( "securityChecks",
                        false,
                        allocator);
    document.AddMember( "OCIBundleDir",
                        rapidjson::Value{bundleDir.c_str(), allocator},
                        allocator);
    document.AddMember( "rootfsFolder",
                        rapidjson::Value{"rootfs", allocator},
                        allocator);
    document.AddMember( "prefixDir",
                        rapidjson::Value{prefixDir.c_str(), allocator},
                        allocator);
    document.AddMember( "dirOfFilesToCopyInContainerEtc",
                        rapidjson::Value{dirOfFilesToCopyInContainerEtc.c_str(), allocator},
                        allocator);
    document.AddMember( "tempDir",
                        rapidjson::Value{"/tmp", allocator},
                        allocator);
    document.AddMember( "localRepositoryBaseDir",
                        rapidjson::Value{localRepositoryBaseDir.c_str(), allocator},
                        allocator);
    document.AddMember( "ramFilesystemType",
                        rapidjson::Value{"ramfs"},
                        allocator);
    document.AddMember( "mksquashfsPath",
                        rapidjson::Value{"/usr/bin/mksquashfs", allocator},
                        allocator);
    document.AddMember( "runcPath",
                        rapidjson::Value{"/usr/bin/runc.amd64", allocator},
                        allocator);

    // siteMounts
    rapidjson::Value siteMountsValue(rapidjson::kArrayType);
    rapidjson::Value mountValue(rapidjson::kObjectType);
    mountValue.AddMember(   "type",
                            rapidjson::Value{"bind", allocator},
                            allocator);
    mountValue.AddMember(   "source",
                            rapidjson::Value{"/source", allocator},
                            allocator);
    mountValue.AddMember(   "destination",
                            rapidjson::Value{"/destination", allocator},
                            allocator);
    rapidjson::Value flagsValue(rapidjson::kObjectType);
    flagsValue.AddMember(   "bind-propagation",
                            rapidjson::Value{"slave", allocator},
                            allocator);
    mountValue.AddMember(   "flags",
                            flagsValue,
                            allocator);
    siteMountsValue.PushBack(mountValue, allocator);
    document.AddMember("siteMounts", siteMountsValue, allocator);

    // userMounts
    rapidjson::Value userMountsValue(rapidjson::kObjectType);
    rapidjson::Value disallowWithPrefixValue(rapidjson::kArrayType);
    disallowWithPrefixValue.PushBack("/etc", allocator);
    disallowWithPrefixValue.PushBack("/var", allocator);
    disallowWithPrefixValue.PushBack("/opt/sarus", allocator);
    userMountsValue.AddMember("notAllowedPrefixesOfPath", disallowWithPrefixValue, allocator);
    rapidjson::Value disallowExactValue(rapidjson::kArrayType);
    disallowExactValue.PushBack("/opt", allocator);
    userMountsValue.AddMember("notAllowedPaths", disallowExactValue, allocator);
    document.AddMember("userMounts", userMountsValue, allocator);
}

ConfigRAII makeConfig() {
    auto raii = ConfigRAII{};
    raii.config = std::make_shared<common::Config>();

    populateJSON(raii.config->json);

    auto dirOfFilesToCopyInContainerEtc = boost::filesystem::path{ raii.config->json["dirOfFilesToCopyInContainerEtc"].GetString() };
    common::createFoldersIfNecessary(dirOfFilesToCopyInContainerEtc);
    common::executeCommand("getent passwd >" + dirOfFilesToCopyInContainerEtc.string() + "/passwd");

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
