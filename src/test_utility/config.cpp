/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
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
    auto hooksDir = prefixDir / "etc/hooks.d";
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
    document.AddMember( "hooksDir",
                        rj::Value{hooksDir.c_str(), allocator},
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

    if(boost::filesystem::exists("/usr/bin/mksquashfs")) {
        document.AddMember( "mksquashfsPath",
                            rj::Value{"/usr/bin/mksquashfs", allocator},
                            allocator);
    }
    else if(boost::filesystem::exists("/usr/sbin/mksquashfs")) {
        document.AddMember( "mksquashfsPath",
                            rj::Value{"/usr/sbin/mksquashfs", allocator},
                            allocator);
    }
    else {
        SARUS_THROW_ERROR("Failed to find mksquashfs on the system."
                          " Hint: either install squashfs-tools or extend this test code"
                          " adding the path where mksquashfs is installed.");
    }

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
                            rj::Value{"rprivate", allocator},
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
}

void createOCIHook(const boost::filesystem::path& hooksDir) {
    common::createFoldersIfNecessary(hooksDir);

    auto hook = hooksDir / "test-hook.json";
    auto os = std::ofstream{ hook.c_str() };

    os <<
    "{"
    "   \"version\": \"1.0.0\","
    "   \"hook\": {"
    "       \"path\": \"/dir/test_hook\","
    "       \"args\": [\"test_hook\", \"arg\"],"
    "       \"env\": ["
    "           \"KEY0=VALUE0\","
    "           \"KEY1=VALUE1\""
    "       ]"
    "   },"
    "   \"when\": {"
    "       \"always\": true"
    "   },"
    "   \"stages\": [\"prestart\", \"poststop\"]"
    "}";
}

ConfigRAII makeConfig() {
    auto raii = ConfigRAII{};
    raii.config = std::make_shared<common::Config>();

    populateJSON(raii.config->json);

    auto prefixDir = boost::filesystem::path{ raii.config->json["prefixDir"].GetString() };

    // passwd + group
    common::createFoldersIfNecessary(prefixDir / "etc");
    common::executeCommand("getent passwd >" + prefixDir.string() + "/etc/passwd");
    common::executeCommand("getent group >" + prefixDir.string() + "/etc/group");

    // JSON schemas
    auto repoRootDir = boost::filesystem::path{__FILE__}.parent_path().parent_path().parent_path();
    common::copyFile(repoRootDir / "definitions.schema.json", prefixDir / "etc/definitions.schema.json");
    common::copyFile(repoRootDir / "sarus.schema.json", prefixDir / "etc/sarus.schema.json");
    common::copyFile(repoRootDir / "hook.schema.json", prefixDir / "etc/hook.schema.json");

    // hooks
    createOCIHook(raii.config->json["hooksDir"].GetString());

    // directories
    raii.config->directories.initialize(false, *raii.config);

    // image + metadata
    raii.config->imageID = common::ImageID{"test", "test", "test", "test_image"};
    common::createFileIfNecessary(raii.config->getMetadataFileOfImage());
    std::ofstream metadataStream(raii.config->getMetadataFileOfImage().c_str());
    metadataStream << "{}";
    metadataStream.close();

    raii.config->commandRun.hostEnvironment = {{"key", "value"}};
    raii.config->commandRun.bundleAnnotations = {{"com.test.dummy_key", "dummy_value"}};

    return raii;
}

}
}
