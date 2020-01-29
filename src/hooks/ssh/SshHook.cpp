/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "SshHook.hpp"

#include <fstream>
#include <cstdlib>
#include <tuple>
#include <algorithm>
#include <grp.h>
#include <sys/prctl.h>
#include <boost/format.hpp>

#include "common/Config.hpp"
#include "common/Error.hpp"
#include "common/Utility.hpp"
#include "common/Lockfile.hpp"
#include "common/PasswdDB.hpp"
#include "runtime/mount_utilities.hpp"
#include "hooks/common/Utility.hpp"


namespace sarus {
namespace hooks {
namespace ssh {

SshHook::SshHook(std::shared_ptr<sarus::common::Config> config): config(config) {}

void SshHook::generateSshKeys(bool overwriteSshKeysIfExist) {
    uidOfUser = getuid(); // keygen command is executed with user identity
    gidOfUser = getgid();
    config->userIdentity.uid = uidOfUser;
    config->userIdentity.gid = gidOfUser;
    localRepositoryDir = sarus::common::getLocalRepositoryDirectory(*config);
    opensshDirInHost = sarus::common::getEnvironmentVariable("SARUS_OPENSSH_DIR");

    sarus::common::Lockfile lock{ getKeysDirInLocalRepository() }; // protect keys from concurrent writes
    if(localRepositoryHasSshKeys() && !overwriteSshKeysIfExist) {
        auto message = boost::format("SSH keys not generated because they already exist in %s."
                                     " Use the '--overwrite' option to overwrite the existing keys.")
                        % getKeysDirInLocalRepository().string();
        logMessage(message, sarus::common::LogLevel::GENERAL);
        return;
    }
    boost::filesystem::remove_all(getKeysDirInLocalRepository());
    sarus::common::createFoldersIfNecessary(getKeysDirInLocalRepository());
    sshKeygen(getKeysDirInLocalRepository() / "ssh_host_rsa_key");
    sshKeygen(getKeysDirInLocalRepository() / "id_rsa");
    logMessage("Successfully generated SSH keys", sarus::common::LogLevel::GENERAL);
}

void SshHook::checkLocalRepositoryHasSshKeys() {
    localRepositoryDir = sarus::common::getEnvironmentVariable("SARUS_LOCAL_REPOSITORY_DIR");
    if(!localRepositoryHasSshKeys()) {
        exit(EXIT_FAILURE); // exit with non-zero to communicate missing keys to calling process (sarus)
    }
}

void SshHook::startSshd() {
    opensshDirInHost = sarus::common::getEnvironmentVariable("SARUS_OPENSSH_DIR");
    std::tie(bundleDir, pidOfContainer) = hooks::common::utility::parseStateOfContainerFromStdin();
    hooks::common::utility::enterNamespacesOfProcess(pidOfContainer);
    parseConfigJSONOfBundle();
    if(!isHookEnabled) {
        return;
    }
    config->userIdentity.uid = uidOfUser;
    config->userIdentity.gid = gidOfUser;
    localRepositoryDir = sarus::common::getLocalRepositoryDirectory(*config);
    sarus::common::copyFolder(opensshDirInHost, opensshDirInBundle);
    copyKeysIntoBundle();
    sarus::common::createFoldersIfNecessary(rootfsDir / "tmp/var/empty"); // sshd's chroot folder
    patchPasswdIfNecessary();
    startSshdInContainer();
    bindMountSshBinary();
}

void SshHook::parseConfigJSONOfBundle() {
    auto json = sarus::common::readJSON(bundleDir / "config.json");

    // get rootfs
    rootfsDir = bundleDir / json["root"]["path"].GetString();
    opensshDirInBundle = rootfsDir / "opt/sarus/openssh";

    // get uid + gid of user
    uidOfUser = json["process"]["user"]["uid"].GetInt();
    gidOfUser = json["process"]["user"]["gid"].GetInt();

    // get environment variables
    auto env = hooks::common::utility::parseEnvironmentVariablesFromOCIBundle(bundleDir);
    if(env["SARUS_SSH_HOOK"] == "1") {
        isHookEnabled = true;
    }
}

bool SshHook::localRepositoryHasSshKeys() const {
    auto expectedKeyFiles = std::vector<std::string>{"ssh_host_rsa_key", "ssh_host_rsa_key.pub", "id_rsa", "id_rsa.pub"};
    for(const auto& file : expectedKeyFiles) {
        if(!boost::filesystem::exists(getKeysDirInLocalRepository() / file)) {
            return false;
        }
    }
    return true;
}

boost::filesystem::path SshHook::getKeysDirInLocalRepository() const {
    return localRepositoryDir / "ssh";
}

void SshHook::sshKeygen(const boost::filesystem::path& outputFile) const {
    auto command = boost::format("%s/bin/ssh-keygen -t rsa -f %s -N \"\"")
        % opensshDirInHost
        % outputFile;
    sarus::common::executeCommand(command.str());
}

void SshHook::copyKeysIntoBundle() const {
    // server keys
    sarus::common::copyFile(localRepositoryDir / "ssh/ssh_host_rsa_key",
                            opensshDirInBundle / "etc/ssh_host_rsa_key",
                            uidOfUser, gidOfUser);
    sarus::common::copyFile(localRepositoryDir / "ssh/ssh_host_rsa_key.pub",
                            opensshDirInBundle / "etc/ssh_host_rsa_key.pub",
                            uidOfUser, gidOfUser);

    // user keys
    boost::filesystem::remove_all(opensshDirInBundle / "etc/userkeys");
    sarus::common::copyFile(localRepositoryDir / "ssh/id_rsa",
                            opensshDirInBundle / "etc/userkeys/id_rsa",
                            uidOfUser, gidOfUser);
    sarus::common::copyFile(localRepositoryDir / "ssh/id_rsa.pub",
                            opensshDirInBundle / "etc/userkeys/id_rsa.pub",
                            uidOfUser, gidOfUser);
    sarus::common::copyFile(localRepositoryDir / "ssh/id_rsa.pub",
                            opensshDirInBundle / "etc/userkeys/authorized_keys",
                            uidOfUser, gidOfUser);
}

void SshHook::bindMountSshBinary() const {
    sarus::common::createFileIfNecessary(rootfsDir / "usr/bin/ssh");
    runtime::bindMount(opensshDirInBundle / "bin/ssh", rootfsDir / "usr/bin/ssh");
}

void SshHook::patchPasswdIfNecessary() const {
    auto passwd = sarus::common::PasswdDB{};
    passwd.read(rootfsDir / "etc/passwd");
    for(auto& entry : passwd.getEntries()) {
        if( entry.userCommandInterpreter
            && !boost::filesystem::exists(rootfsDir / *entry.userCommandInterpreter)) {
            entry.userCommandInterpreter = "/bin/sh";
        }
    }
    passwd.write(rootfsDir / "etc/passwd");
}

void SshHook::startSshdInContainer() const {
    std::function<void()> preExecActions = [this]() {
        if(chroot(rootfsDir.c_str()) != 0) {
            auto message = boost::format("Failed to chroot to %s: %s")
                % rootfsDir % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }

        // drop capabilities
        // from capability zero onwards until prctl() fails because we went beyond the last valid capability
        for(int capIdx = 0; ; capIdx++) {
            if (prctl(PR_CAPBSET_DROP, capIdx, 0, 0, 0) != 0) {
                if(errno == EINVAL) {
                    break; // reached end of valid capabilities
                }
                else {
                    auto message = boost::format("Failed to prctl(PR_CAPBSET_DROP, %d, 0, 0, 0): %s")
                        % capIdx % strerror(errno);
                    SARUS_THROW_ERROR(message.str());
                }
            }
        }

        // drop supplementary groups (if any)
        if(setgroups(0, NULL) != 0) {
            auto message = boost::format("Failed to setgroups(0, NULL): %s") % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }

        // change to user's gid
        if(setresgid(gidOfUser, gidOfUser, gidOfUser) != 0) {
            auto message = boost::format("Failed to setresgid(%1%, %1%, %1%): %2%") % gidOfUser % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }

        // change to user's uid
        if(setresuid(uidOfUser, uidOfUser, uidOfUser) != 0) {
            auto message = boost::format("Failed to setresuid(%1%, %1%, %1%): %2%") % uidOfUser % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }

        // set NoNewPrivs
        if(prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
            auto message = boost::format("Failed to prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0): %s") % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }
    };

    auto status = sarus::common::forkExecWait({"/opt/sarus/openssh/sbin/sshd"}, preExecActions);
    if(status != 0) {
        auto message = boost::format("/opt/sarus/openssh/sbin/sshd exited with status %d")
            % status;
        SARUS_THROW_ERROR(message.str());
    }
}

void SshHook::logMessage(const boost::format& message, sarus::common::LogLevel level,
                         std::ostream& out, std::ostream& err) const {
    logMessage(message.str(), level, out, err);
}

void SshHook::logMessage(const std::string& message, sarus::common::LogLevel level,
                         std::ostream& out, std::ostream& err) const {
    auto subsystemName = "ssh-hook";
    sarus::common::Logger::getInstance().log(message, subsystemName, level, out, err);
}

}}} // namespace
