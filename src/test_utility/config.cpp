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

static void populateJSON(rapidjson::Document& document) {
    auto& allocator = document.GetAllocator();

    auto bundleDir = common::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("./sarus-test-ocibundle"));
    auto dirOfFilesToCopyInContainerEtc = common::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("./sarus-test-dirOfFilesToCopyInContainerEtc"));

    document.AddMember( "securityChecks",
                        false,
                        allocator);
    document.AddMember( "OCIBundleDir",
                        rapidjson::Value{bundleDir.c_str(), allocator},
                        allocator);
    document.AddMember( "rootfsFolder",
                        rapidjson::Value{"rootfs", allocator},
                        allocator);
    document.AddMember( "dirOfFilesToCopyInContainerEtc",
                        rapidjson::Value{dirOfFilesToCopyInContainerEtc.c_str(), allocator},
                        allocator);
    document.AddMember( "tempDir",
                        rapidjson::Value{"/tmp", allocator},
                        allocator);
    document.AddMember( "localRepositoryBaseDir",
                        rapidjson::Value{"/home", allocator},
                        allocator);
    document.AddMember( "ramFilesystemType",
                        rapidjson::Value{"ramfs"},
                        allocator);
    document.AddMember( "mksquashfsPath",
                        rapidjson::Value{"/usr/bin/mksquashfs", allocator},
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

sarus::common::Config makeConfig() {
    auto config = common::Config{};

    populateJSON(config.json);

    auto repository = common::makeUniquePathWithRandomSuffix(boost::filesystem::absolute("./sarus-test-repository"));
    config.directories.repository = repository;
    config.directories.temp   = config.directories.repository / "temp";
    config.directories.cache = config.directories.repository / "cache";
    config.directories.images = config.directories.repository / "images";

    config.commandRun.hostEnvironment = {{"key", "value"}};

    return config;
}

}
}
