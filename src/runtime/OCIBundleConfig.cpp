/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "OCIBundleConfig.hpp"

#include <fstream>
#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

#include "libsarus/Logger.hpp"
#include "libsarus/Utility.hpp"
#include "common/GroupDB.hpp"
#include "common/ImageMetadata.hpp"
#include "runtime/Utility.hpp"
#include "runtime/OCIHooksFactory.hpp"


namespace rj = rapidjson;

namespace sarus {
namespace runtime {

OCIBundleConfig::OCIBundleConfig(std::shared_ptr<const common::Config> config)
    : config{config}
    , configsMerger{config, sarus::common::ImageMetadata{config->getMetadataFileOfImage(), config->userIdentity}}
    , document{new rj::Document{}}
    , allocator{&document->GetAllocator()}
    , configFile{boost::filesystem::path{config->json["OCIBundleDir"].GetString()} / "config.json"}
{}

void OCIBundleConfig::generateConfigFile() const {
    utility::logMessage("Generating bundle's config file", libsarus::LogLevel::INFO);
    makeJsonDocument();
    libsarus::filesystem::createFileIfNecessary(configFile);
    boost::filesystem::permissions(configFile, boost::filesystem::perms::owner_read |
                                               boost::filesystem::perms::owner_write);
    libsarus::json::write(*document, configFile);
    utility::logMessage("Successfully generated bundle's config file", libsarus::LogLevel::INFO);
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

    // apparmorProfile
    if(config->json.HasMember("apparmorProfile")) {
        auto profile = std::string{config->json["apparmorProfile"].GetString()};
        if(!isAppArmorProfileLoaded(profile)) {
            auto message = boost::format("AppArmor profile '%s' was configured for use but it's not "
                                         "loaded into the kernel. "
                                         "Please contact your system administrator.");
            SARUS_THROW_ERROR(message.str());
        }
        process.AddMember("apparmorProfile",
                          rj::Value{profile.c_str(), *allocator},
                          *allocator);
    }

    // selinuxLabel
    if(config->json.HasMember("selinuxLabel")) {
        process.AddMember("selinuxLabel",
                          rj::Value{config->json["selinuxLabel"].GetString(), *allocator},
                          *allocator);
    }

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
            libsarus::Logger::getInstance().log(message, "Runtime", libsarus::LogLevel::WARN);
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
    // sys/fs/cgroup
    {
        auto element = rj::Value{rj::kObjectType};
        element.AddMember("destination", rj::Value{"/sys/fs/cgroup"}, *allocator);
        element.AddMember("type", rj::Value{"cgroup"}, *allocator);
        element.AddMember("source", rj::Value{"cgroup"}, *allocator);

        auto options = rj::Value{rj::kArrayType};
        options.PushBack(rj::Value{"nosuid"}, *allocator);
        options.PushBack(rj::Value{"noexec"}, *allocator);
        options.PushBack(rj::Value{"nodev"}, *allocator);
        options.PushBack(rj::Value{"relatime"}, *allocator);
        options.PushBack(rj::Value{"ro"}, *allocator);
        element.AddMember("options", options, *allocator);

        mounts.PushBack(element, *allocator);
    }

    return mounts;
}

rj::Value OCIBundleConfig::makeMemberLinux() const {
    auto linuxV = rj::Value{rj::kObjectType};
    // resources
    {
        auto resources = rj::Value{rj::kObjectType};

        // cpu
        //
        // Slurm performs the CPU pinning of the host process through sched_setaffinity(2),
        // instead of modifying the cpuset cgroup. See Slurm's code and explanation here:
        // https://github.com/SchedMD/slurm/blob/44e651a5d1f688ec012d0bc5c0c9dd4a0df8ee94/src/plugins/task/cgroup/task_cgroup_cpuset.c#L1227
        //
        // Because Slurm modifies the host process through sched_setaffinity(2), the resulting
        // CPU pinning might be different from the host process' cpuset cgroup. If this happens,
        // the OCI runtime could take the "cpuset" cgroup of the host process, apply it as it is
        // to the container process and by doing so the CPU pinning previously performed by Slurm
        // with sched_setaffinity(2) may be removed. This issue was observed while using runc, as
        // well as crun.
        //
        // To fix the issue and make sure that we preserve the Slurm's CPU pinning inside the
        // container, we explicitely specify the cpuset cgroup in the OCI bundle's config file
        // with the values obtained from sched_getaffinity(2).
        auto cpu = rj::Value{rj::kObjectType};
        auto intToString = boost::adaptors::transformed([](int i) { return std::to_string(i); });
        auto cpus = boost::join(config->commandRun.cpuAffinity |intToString, ",");
        auto cpuAffinity = rj::Value{cpus.c_str(), *allocator};
        cpu.AddMember("cpus", cpuAffinity, *allocator);

        // devices
        auto devices = rj::Value{rj::kArrayType};
        auto denyAllRule = rj::Value{rj::kObjectType};
        denyAllRule.AddMember("allow", rj::Value{false}, *allocator);
        denyAllRule.AddMember("access", rj::Value{"rwm"}, *allocator);
        devices.PushBack(denyAllRule, *allocator);
        for (const auto& device : config->commandRun.deviceMounts) {
            auto deviceRule = rj::Value{rj::kObjectType};
            const auto type = std::string{device->getType()};
            deviceRule.AddMember("allow",  rj::Value{true}, *allocator);
            deviceRule.AddMember("type",   rj::Value{type.c_str(), *allocator}, *allocator);
            deviceRule.AddMember("major",  rj::Value{device->getMajorID()}, *allocator);
            deviceRule.AddMember("minor",  rj::Value{device->getMinorID()}, *allocator);
            deviceRule.AddMember("access", rj::Value{device->getAccess().string().c_str(), *allocator}, *allocator);
            devices.PushBack(deviceRule, *allocator);
        }

        resources.AddMember("cpu", cpu, *allocator);
        resources.AddMember("devices", devices, *allocator);
        linuxV.AddMember("resources", resources, *allocator);
    }
    // namespaces
    {
        auto namespaces = rj::Value{rj::kArrayType};

        auto mount = rj::Value{rj::kObjectType};
        mount.AddMember("type", rj::Value{"mount"}, *allocator);
        namespaces.PushBack(mount, *allocator);

        if(config->commandRun.createNewPIDNamespace) {
            auto pid = rj::Value{rj::kObjectType};
            pid.AddMember("type", rj::Value{"pid"}, *allocator);
            namespaces.PushBack(pid, *allocator);
        }

        linuxV.AddMember("namespaces", namespaces, *allocator);
    }
    // rootfsPropagation
    linuxV.AddMember("rootfsPropagation", rj::Value("slave"), *allocator);
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
        linuxV.AddMember("maskedPaths", paths, *allocator);
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
        linuxV.AddMember("readonlyPaths", paths, *allocator);
    }
    // seccomp
    {
        if(config->json.HasMember("seccompProfile")) {
            auto seccompProfilePath = boost::filesystem::path{ config->json["seccompProfile"].GetString() };
            if(!boost::filesystem::is_regular_file(seccompProfilePath)) {
                auto message = boost::format("The path configured for the container's seccomp profile does "
                                             "not correspond to a regular file: %s . "
                                             "The seccomp profile must be a JSON file defining an OCI-compliant "
                                             "seccomp object. "
                                             "Please contact your system administrator.") % seccompProfilePath;
                SARUS_THROW_ERROR(message.str());
            }

            auto seccompProfile = rj::Document{};
            try {
                seccompProfile.CopyFrom(libsarus::json::read(seccompProfilePath), *allocator);
            }
            catch(libsarus::Error& e) {
                auto message = boost::format("Error reading seccomp profile: %s\n"
                                             "The seccomp profile must be a JSON file defining an OCI-compliant "
                                             "'seccomp' object. "
                                             "Please contact your system administrator.") % e.what();
                SARUS_RETHROW_ERROR(e, message.str());
            }
            linuxV.AddMember("seccomp", seccompProfile, *allocator);
        }
    }
    // mountLabel
    {
        if(config->json.HasMember("selinuxMountLabel")) {
            linuxV.AddMember("mountLabel",
                            rj::Value{config->json["selinuxMountLabel"].GetString(), *allocator},
                            *allocator);
        }
    }
    return linuxV;
}

rj::Value OCIBundleConfig::makeMemberHooks() const {
    auto jsonHooks = rj::Value{rj::kObjectType};

    if(!config->json.HasMember("hooksDir")) {
        utility::logMessage("Skipping OCI hooks configuration (\"hooksDir\" is not set in Sarus' config)",
                            libsarus::LogLevel::INFO);
        return jsonHooks;
    }

    auto hooksDir = boost::filesystem::path{ config->json["hooksDir"].GetString() };
    auto schemaFile = boost::filesystem::path{ config->json["prefixDir"].GetString() } / "etc/hook.schema.json";

    for(const auto& hook : OCIHooksFactory{}.createHooks(hooksDir, schemaFile)) {
        if(hook.isActive(config)) {
            for(const auto& stage : hook.stages) {
                if(!jsonHooks.HasMember(stage.c_str())) {
                    auto key = rj::Value{stage.c_str(), *allocator};
                    jsonHooks.AddMember(key, rj::Value{rj::kArrayType}, *allocator);
                }
                auto jsonHook = rj::Document{};
                jsonHook.CopyFrom(hook.jsonHook, *allocator);
                jsonHooks[stage.c_str()].PushBack(jsonHook, *allocator);
            }
        }
    }

    return jsonHooks;
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

bool OCIBundleConfig::isAppArmorProfileLoaded(const std::string& profile) const {
    auto loadedProfilesPath = boost::filesystem::path("/sys/kernel/security/apparmor/profiles");
    if(!boost::filesystem::exists(loadedProfilesPath)) {
      auto message = boost::format("Use of an AppArmor profile was configured but Sarus could not"
                                   "find the loaded profiles list at %s .\n"
                                   "Please ensure that AppArmor is enabled and that the Linux "
                                   "kernel's securityfs filesystem is mounted.") % loadedProfilesPath;
      SARUS_THROW_ERROR(message.str());
    }

    boost::filesystem::ifstream loadedProfiles{loadedProfilesPath};
    for(std::string loaded; std::getline(loadedProfiles, loaded); ) {
        if(profile == loaded) {
            return true;
        }
    }
    return false;
}

}
}
