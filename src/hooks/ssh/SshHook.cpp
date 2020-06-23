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

#include "common/Error.hpp"
#include "common/Utility.hpp"
#include "common/Lockfile.hpp"
#include "common/PasswdDB.hpp"
#include "runtime/mount_utilities.hpp"
#include "hooks/common/Utility.hpp"


namespace sarus {
namespace hooks {
namespace ssh {

void SshHook::generateSshKeys(bool overwriteSshKeysIfExist) {
    log("Generating SSH keys", sarus::common::LogLevel::INFO);

    uidOfUser = getuid(); // keygen command is executed with user identity
    gidOfUser = getgid();
    sshKeysDir = getSshKeysDir();
    opensshDirInHost = sarus::common::getEnvironmentVariable("OPENSSH_DIR");

    sarus::common::createFoldersIfNecessary(sshKeysDir);
    sarus::common::Lockfile lock{ sshKeysDir }; // protect keys from concurrent writes

    if(userHasSshKeys() && !overwriteSshKeysIfExist) {
        auto message = boost::format("SSH keys not generated because they already exist in %s."
                                     " Use the '--overwrite' option to overwrite the existing keys.")
                        % sshKeysDir;
        log(message, sarus::common::LogLevel::GENERAL);
        return;
    }

    boost::filesystem::remove_all(sshKeysDir);
    sarus::common::createFoldersIfNecessary(sshKeysDir);
    sshKeygen(sshKeysDir / "ssh_host_rsa_key");
    sshKeygen(sshKeysDir / "id_rsa");

    log("Successfully generated SSH keys", sarus::common::LogLevel::GENERAL);
    log("Successfully generated SSH keys", sarus::common::LogLevel::INFO);
}

void SshHook::checkUserHasSshKeys() {
    log("Checking that user has SSH keys", sarus::common::LogLevel::INFO);

    uidOfUser = getuid(); // "user-has-ssh-keys" command is executed with user identity
    gidOfUser = getgid();
    sshKeysDir = getSshKeysDir();

    if(!userHasSshKeys()) {
        log(boost::format{"Could not find SSH keys in %s"} % sshKeysDir, sarus::common::LogLevel::INFO);
        exit(EXIT_FAILURE); // exit with non-zero to communicate missing keys to calling process (sarus)
    }

    log("Successfully checked that user has SSH keys", sarus::common::LogLevel::INFO);
}

void SshHook::startSshd() {
    log("Activating SSH in container", sarus::common::LogLevel::INFO);

    opensshDirInHost = sarus::common::getEnvironmentVariable("OPENSSH_DIR");
    std::tie(bundleDir, pidOfContainer) = hooks::common::utility::parseStateOfContainerFromStdin();
    hooks::common::utility::enterNamespacesOfProcess(pidOfContainer);
    parseConfigJSONOfBundle();
    sshKeysDir = getSshKeysDir();
    sarus::common::copyFolder(opensshDirInHost, opensshDirInBundle);
    copyKeysIntoBundle();
    sarus::common::createFoldersIfNecessary(rootfsDir / "tmp/var/empty"); // sshd's chroot folder
    patchPasswdIfNecessary();
    startSshdInContainer();
    bindMountSshBinary();

    log("Successfully activated SSH in container", sarus::common::LogLevel::INFO);
}

void SshHook::parseConfigJSONOfBundle() {
    log("Parsing bundle's config.json", sarus::common::LogLevel::INFO);

    auto json = sarus::common::readJSON(bundleDir / "config.json");

    hooks::common::utility::applyLoggingConfigIfAvailable(json);

    // get rootfs
    auto root = boost::filesystem::path{ json["root"]["path"].GetString() };
    if(root.is_absolute()) {
        rootfsDir = root;
    }
    else {
        rootfsDir = bundleDir / root;
    }

    opensshDirInBundle = rootfsDir / "opt/sarus/openssh";

    // get uid + gid of user
    uidOfUser = json["process"]["user"]["uid"].GetInt();
    gidOfUser = json["process"]["user"]["gid"].GetInt();

    log("Successfully parsed bundle's config.json", sarus::common::LogLevel::INFO);
}

bool SshHook::userHasSshKeys() const {
    auto expectedKeyFiles = std::vector<std::string>{"ssh_host_rsa_key", "ssh_host_rsa_key.pub", "id_rsa", "id_rsa.pub"};
    for(const auto& file : expectedKeyFiles) {
        auto fullPath = sshKeysDir / file;
        if(!boost::filesystem::exists(fullPath)) {
            auto message = boost::format{"Expected SSH key file %s not found"} % fullPath;
            log(message, sarus::common::LogLevel::DEBUG);
            return false;
        }
    }
    log(boost::format{"Found SSH keys in %s"} % sshKeysDir, sarus::common::LogLevel::DEBUG);
    return true;
}

boost::filesystem::path SshHook::getSshKeysDir() const {
    auto baseDir = boost::filesystem::path{ sarus::common::getEnvironmentVariable("HOOK_BASE_DIR") };
    auto passwdFile = boost::filesystem::path{ sarus::common::getEnvironmentVariable("PASSWD_FILE") };
    auto username = sarus::common::PasswdDB{passwdFile}.getUsername(uidOfUser);
    return baseDir / username / ".oci-hooks/ssh/keys";
}

void SshHook::sshKeygen(const boost::filesystem::path& outputFile) const {
    auto command = boost::format("%s/bin/ssh-keygen -t rsa -f %s -N \"\"")
        % opensshDirInHost
        % outputFile;
    log(boost::format{"Executing command '%s'"} % command, sarus::common::LogLevel::DEBUG);
    sarus::common::executeCommand(command.str());
}

void SshHook::copyKeysIntoBundle() const {
    log("Copying SSH keys into bundle", sarus::common::LogLevel::INFO);

    // server keys
    sarus::common::copyFile(sshKeysDir / "ssh_host_rsa_key",
                            opensshDirInBundle / "etc/ssh_host_rsa_key",
                            uidOfUser, gidOfUser);
    sarus::common::copyFile(sshKeysDir / "ssh_host_rsa_key.pub",
                            opensshDirInBundle / "etc/ssh_host_rsa_key.pub",
                            uidOfUser, gidOfUser);

    // user keys
    boost::filesystem::remove_all(opensshDirInBundle / "etc/userkeys");
    sarus::common::copyFile(sshKeysDir / "id_rsa",
                            opensshDirInBundle / "etc/userkeys/id_rsa",
                            uidOfUser, gidOfUser);
    sarus::common::copyFile(sshKeysDir / "id_rsa.pub",
                            opensshDirInBundle / "etc/userkeys/id_rsa.pub",
                            uidOfUser, gidOfUser);
    sarus::common::copyFile(sshKeysDir / "id_rsa.pub",
                            opensshDirInBundle / "etc/userkeys/authorized_keys",
                            uidOfUser, gidOfUser);

    log("Successfully copied SSH keys into bundle", sarus::common::LogLevel::INFO);
}

void SshHook::bindMountSshBinary() const {
    sarus::common::createFileIfNecessary(rootfsDir / "usr/bin/ssh");
    runtime::bindMount(opensshDirInBundle / "bin/ssh", rootfsDir / "usr/bin/ssh");
}

void SshHook::patchPasswdIfNecessary() const {
    log("Patching container's /etc/passwd if necessary"
        " (ensure that command interpreter is valid)", sarus::common::LogLevel::INFO);

    auto passwd = sarus::common::PasswdDB{rootfsDir / "etc/passwd"};
    for(auto& entry : passwd.getEntries()) {
        if( entry.userCommandInterpreter
            && !boost::filesystem::exists(rootfsDir / *entry.userCommandInterpreter)) {
            entry.userCommandInterpreter = "/bin/sh";
        }
    }
    passwd.write(rootfsDir / "etc/passwd");

    log("Successfully patched container's /etc/passwd", sarus::common::LogLevel::INFO);
}

void SshHook::startSshdInContainer() const {
    log("Starting sshd in container", sarus::common::LogLevel::INFO);

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

    log("Successfully started sshd in container", sarus::common::LogLevel::INFO);
}

void SshHook::log(const boost::format& message, sarus::common::LogLevel level) const {
    log(message.str(), level);
}

void SshHook::log(const std::string& message, sarus::common::LogLevel level) const {
    auto subsystemName = "SSH hook";
    sarus::common::Logger::getInstance().log(message, subsystemName, level);
}

}}} // namespace
