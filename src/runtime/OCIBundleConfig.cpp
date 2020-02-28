/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "OCIBundleConfig.hpp"

#include <fstream>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

#include "common/Logger.hpp"
#include "common/Utility.hpp"
#include "common/GroupDB.hpp"
#include "common/ImageMetadata.hpp"
#include "runtime/Utility.hpp"


namespace rj = rapidjson;

namespace sarus {
namespace runtime {

OCIBundleConfig::OCIBundleConfig(std::shared_ptr<const common::Config> config)
    : config{config}
    , configsMerger{config, common::ImageMetadata{config->getMetadataFileOfImage()}}
    , document{new rj::Document{}}
    , allocator{&document->GetAllocator()}
    , configFile{boost::filesystem::path{config->json["OCIBundleDir"].GetString()} / "config.json"}
{}

void OCIBundleConfig::generateConfigFile() const {
    utility::logMessage("Generating bundle's config file", common::LogLevel::INFO);
    makeJsonDocument();
    common::createFileIfNecessary(configFile);
    boost::filesystem::permissions(configFile, boost::filesystem::perms::owner_read |
                                               boost::filesystem::perms::owner_write);
    common::writeJSON(*document, configFile);
    utility::logMessage("Successfully generated bundle's config file", common::LogLevel::INFO);
}

const boost::filesystem::path& OCIBundleConfig::getConfigFile() const {
    return configFile;
}

void OCIBundleConfig::makeJsonDocument() const {
    document->SetObject();
    document->AddMember("ociVersion",
                        rj::Value{"1.0.0", *allocator},
                        *allocator);
    document->AddMember("process",
                        makeMemberProcess(),
                        *allocator);
    document->AddMember("root",
                        makeMemberRoot(),
                        *allocator);
    document->AddMember("mounts",
                        makeMemberMounts(),
                        *allocator);
    document->AddMember("linux",
                        makeMemberLinux(),
                        *allocator);
    document->AddMember("hooks",
                        makeMemberHooks(),
                        *allocator);
    document->AddMember("annotations",
                        makeMemberAnnotations(),
                        *allocator);
}

rj::Value OCIBundleConfig::makeMemberProcess() const {
    auto process = rj::Value{rj::kObjectType};
    process.AddMember(  "terminal",
                        rj::Value{config->commandRun.allocatePseudoTTY},
                        *allocator);

    // user
    {
        auto user = rj::Value{rj::kObjectType};
        user.AddMember("uid", rj::Value{config->userIdentity.uid}, *allocator);
        user.AddMember("gid", rj::Value{config->userIdentity.gid}, *allocator);

        auto additionalGids = rj::Value{rj::kArrayType};
        for(const auto& gid : config->userIdentity.supplementaryGids) {
            additionalGids.PushBack(rj::Value{gid}, *allocator);
        }
        user.AddMember("additionalGids", additionalGids, *allocator);

        process.AddMember("user", user, *allocator);
    }

    // args
    auto args = rj::Value{rj::kArrayType};
    if(config->commandRun.addInitProcess) {
        args.PushBack(  rj::Value{"/dev/init", *allocator},
                        *allocator);
        args.PushBack(  rj::Value{"--", *allocator},
                        *allocator);
    }
    for(auto* arg : configsMerger.getCommandToExecuteInContainer()) {
        auto value = rj::Value{arg, *allocator};
        args.PushBack(value, *allocator);
    }
    process.AddMember("args", args, *allocator);

    // env
    auto env = rj::Value{rj::kArrayType};
    for(const auto& kv : configsMerger.getEnvironmentInContainer()) {
        auto variable = kv.first + "=" + kv.second;
        env.PushBack(   rj::Value{variable.c_str(), *allocator},
                        *allocator);
    }
    process.AddMember("env", env, *allocator);
    process.AddMember(  "cwd",
                        rj::Value{configsMerger.getWorkdirInContainer().c_str(), *allocator},
                        *allocator);
    process.AddMember("capabilities", rj::Value{rj::kObjectType}, *allocator);
    process.AddMember("noNewPrivileges", rj::Value{true}, *allocator);

    return process;
}

rj::Value OCIBundleConfig::makeMemberRoot() const {
    auto root = rj::Value{rj::kObjectType};
    root.AddMember( "path",
                    rj::Value{config->json["rootfsFolder"].GetString(), *allocator},
                    *allocator);
    root.AddMember( "readonly",
                    rj::Value{false},
                    *allocator);
    return root;
}

rj::Value OCIBundleConfig::makeMemberMounts() const {
    auto mounts = rj::Value{rj::kArrayType};
    // proc
    {
        auto element = rj::Value{rj::kObjectType};
        element.AddMember("destination", rj::Value{"/proc"}, *allocator);
        element.AddMember("type", rj::Value{"proc"}, *allocator);
        element.AddMember("source", rj::Value{"proc"}, *allocator);
        mounts.PushBack(element, *allocator);
    }
    // dev/pts
    {
        auto element = rj::Value{rj::kObjectType};
        element.AddMember("destination", rj::Value{"/dev/pts"}, *allocator);
        element.AddMember("type", rj::Value{"devpts"}, *allocator);
        element.AddMember("source", rj::Value{"devpts"}, *allocator);

        auto options = rj::Value{rj::kArrayType};
        options.PushBack(rj::Value{"nosuid"}, *allocator);
        options.PushBack(rj::Value{"noexec"}, *allocator);
        options.PushBack(rj::Value{"newinstance"}, *allocator);
        options.PushBack(rj::Value{"ptmxmode=0666"}, *allocator);
        options.PushBack(rj::Value{"mode=0620"}, *allocator);
        auto gid = findGidOfTtyGroup();
        if(gid) {
            // Mount /dev/pts with the gid=<gid of tty group> option (typically gid=5).
            // This is a standard setting in a Linux environment and it is needed, otherwise
            // the tty files created in /dev/pts will not be owned by the tty group by
            // default and that could generate errors.
            // E.g.:
            // sshd creates a new tty when a session is started. If the new tty file is not
            // owned by the tty group, sshd does a chown on the tty file. If sshd is being
            // executed as non-root, it will not have the permissions to do the chown and
            // will terminate with an error.
            auto option = "gid=" + std::to_string(*gid);
            options.PushBack(rj::Value{option.c_str(), *allocator}, *allocator);
        }
        else {
            auto message = "Mounting /dev/pts without the gid=<gid of tty group> option, because no tty gid was found."
                           " Some programs, e.g. sshd, might run into errors because of this.";
            common::Logger::getInstance().log(message, "Runtime", common::LogLevel::WARN);
        }
        element.AddMember("options", options, *allocator);

        mounts.PushBack(element, *allocator);
    }
    // dev/shm - bind mounted from host to allow communication between processes that use it
    {
        auto element = rj::Value{rj::kObjectType};
        element.AddMember("destination", rj::Value{"/dev/shm"}, *allocator);
        element.AddMember("type", rj::Value{"bind"}, *allocator);
        element.AddMember("source", rj::Value{"/dev/shm"}, *allocator);

        auto options = rj::Value{rj::kArrayType};
        options.PushBack(rj::Value{"nosuid"}, *allocator);
        options.PushBack(rj::Value{"noexec"}, *allocator);
        options.PushBack(rj::Value{"nodev"}, *allocator);
        options.PushBack(rj::Value{"rbind"}, *allocator);
        options.PushBack(rj::Value{"slave"}, *allocator);
        options.PushBack(rj::Value{"rw"}, *allocator);
        element.AddMember("options", options, *allocator);

        mounts.PushBack(element, *allocator);
    }
    // dev/mqueue
    {
        auto element = rj::Value{rj::kObjectType};
        element.AddMember("destination", rj::Value{"/dev/mqueue"}, *allocator);
        element.AddMember("type", rj::Value{"mqueue"}, *allocator);
        element.AddMember("source", rj::Value{"mqueue"}, *allocator);

        auto options = rj::Value{rj::kArrayType};
        options.PushBack(rj::Value{"nosuid"}, *allocator);
        options.PushBack(rj::Value{"noexec"}, *allocator);
        options.PushBack(rj::Value{"nodev"}, *allocator);
        element.AddMember("options", options, *allocator);

        mounts.PushBack(element, *allocator);
    }
    // sys
    {
        auto element = rj::Value{rj::kObjectType};
        element.AddMember("destination", rj::Value{"/sys"}, *allocator);
        element.AddMember("type", rj::Value{"sysfs"}, *allocator);
        element.AddMember("source", rj::Value{"sysfs"}, *allocator);

        auto options = rj::Value{rj::kArrayType};
        options.PushBack(rj::Value{"nosuid"}, *allocator);
        options.PushBack(rj::Value{"noexec"}, *allocator);
        options.PushBack(rj::Value{"nodev"}, *allocator);
        options.PushBack(rj::Value{"ro"}, *allocator);
        element.AddMember("options", options, *allocator);

        mounts.PushBack(element, *allocator);
    }

    return mounts;
}

rj::Value OCIBundleConfig::makeMemberLinux() const {
    auto linux = rj::Value{rj::kObjectType};
    // resources
    {
        auto resources = rj::Value{rj::kObjectType};
        auto cpu = rj::Value{rj::kObjectType};
        auto cpusAllowedList = rj::Value{config->commandRun.cpusAllowedList.c_str(), *allocator};

        cpu.AddMember("cpus", cpusAllowedList, *allocator);
        resources.AddMember("cpu", cpu, *allocator);
        linux.AddMember("resources", resources, *allocator);
    }
    // namespaces
    {
        auto namespaces = rj::Value{rj::kArrayType};

        auto pid = rj::Value{rj::kObjectType};
        pid.AddMember("type", rj::Value{"pid"}, *allocator);
        namespaces.PushBack(pid, *allocator);

        auto mount = rj::Value{rj::kObjectType};
        mount.AddMember("type", rj::Value{"mount"}, *allocator);
        namespaces.PushBack(mount, *allocator);

        linux.AddMember("namespaces", namespaces, *allocator);
    }
    // rootfsPropagation
    linux.AddMember("rootfsPropagation", rj::Value("slave"), *allocator);
    // maskedPaths
    {
        auto paths = rj::Value{rj::kArrayType};
        paths.PushBack("/proc/kcore", *allocator);
        paths.PushBack("/proc/latency_stats", *allocator);
        paths.PushBack("/proc/timer_list", *allocator);
        paths.PushBack("/proc/timer_stats", *allocator);
        paths.PushBack("/proc/sched_debug", *allocator);
        paths.PushBack("/sys/firmware", *allocator);
        paths.PushBack("/proc/scsi", *allocator);
        linux.AddMember("maskedPaths", paths, *allocator);
    }
    // readonlyPaths
    {
        auto paths = rj::Value{rj::kArrayType};
        paths.PushBack("/proc/asound", *allocator);
        paths.PushBack("/proc/bus", *allocator);
        paths.PushBack("/proc/fs", *allocator);
        paths.PushBack("/proc/irq", *allocator);
        paths.PushBack("/proc/sys", *allocator);
        paths.PushBack("/proc/sysrq-trigger", *allocator);
        linux.AddMember("readonlyPaths", paths, *allocator);
    }
    return linux;
}

rj::Value OCIBundleConfig::makeMemberHooks() const {
    if(config->json.HasMember("OCIHooks")) {
        return rj::Value{config->json["OCIHooks"],
                         *allocator};
    }
    else {
        return rj::Value{rj::kObjectType};
    }
}

rapidjson::Value OCIBundleConfig::makeMemberAnnotations() const {
    auto annotations = rj::Value{rj::kObjectType};

    for(const auto& annotation : configsMerger.getBundleAnnotations()) {
        auto key = rj::Value{annotation.first.c_str(), *allocator};
        auto value = rj::Value{annotation.second.c_str(), *allocator};
        annotations.AddMember(key, value, *allocator);
    }

    return annotations;
}

boost::optional<gid_t> OCIBundleConfig::findGidOfTtyGroup() const {
    auto prefixDir = boost::filesystem::path{config->json["prefixDir"].GetString()};
    auto group = common::GroupDB{};
    group.read(prefixDir / "etc/group");

    for(const auto& entry : group.getEntries()) {
        if(entry.groupName == "tty") {
            return entry.gid;
        }
    }

    return {};
}

}
}
