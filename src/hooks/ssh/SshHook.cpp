/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
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
#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/Utility.hpp"
#include "common/Lockfile.hpp"
#include "common/PasswdDB.hpp"
#include "common/SecurityChecks.hpp"
#include "runtime/mount_utilities.hpp"
#include "hooks/common/Utility.hpp"


namespace sarus {
namespace hooks {
namespace ssh {

void SshHook::generateSshKeys(bool overwriteSshKeysIfExist) {
    uidOfUser = getuid(); // keygen command is executed with user identity
    gidOfUser = getgid();
    auto config = parseConfigJSONOfSarus(uidOfUser, gidOfUser);
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
    auto config = parseConfigJSONOfSarus(uidOfUser, gidOfUser);
    sarus::common::SecurityChecks{config}.checkThatPathIsUntamperable(opensshDirInHost);
    localRepositoryDir = sarus::common::getLocalRepositoryDirectory(*config);
    sarus::common::copyFolder(opensshDirInHost, opensshDirInBundle);
    copyKeysIntoBundle();
    sarus::common::createFoldersIfNecessary(rootfsDir / "tmp/var/empty"); // sshd's chroot folder
    addSshdUserIfNecessary();
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

std::shared_ptr<sarus::common::Config> SshHook::parseConfigJSONOfSarus(uid_t uidOfUser, gid_t gidOfUser) const {
    boost::filesystem::path sarusInstallationPrefixDir = sarus::common::getEnvironmentVariable("SARUS_PREFIX_DIR");
    auto configFile =  sarusInstallationPrefixDir / "etc/sarus.json";
    auto configSchemaFile = sarusInstallationPrefixDir / "etc/sarus.schema.json";
    auto config = std::make_shared<sarus::common::Config>();
    config->userIdentity.uid = uidOfUser;
    config->userIdentity.gid = gidOfUser;
    config->initializeJson(config, configFile, configSchemaFile);
    return config;
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
    boost::filesystem::remove(opensshDirInBundle / "etc/ssh_host_rsa_key");
    boost::filesystem::remove(opensshDirInBundle / "etc/ssh_host_rsa_key.pub");
    boost::filesystem::copy_file(localRepositoryDir / "ssh/ssh_host_rsa_key", opensshDirInBundle / "etc/ssh_host_rsa_key");
    boost::filesystem::copy_file(localRepositoryDir / "ssh/ssh_host_rsa_key.pub", opensshDirInBundle / "etc/ssh_host_rsa_key.pub");

    // user keys
    boost::filesystem::remove_all(opensshDirInBundle / "etc/userkeys");
    sarus::common::createFoldersIfNecessary(opensshDirInBundle / "etc/userkeys");
    boost::filesystem::copy_file(localRepositoryDir / "ssh/id_rsa", opensshDirInBundle / "etc/userkeys/id_rsa");
    sarus::common::setOwner(opensshDirInBundle / "etc/userkeys/id_rsa", uidOfUser, gidOfUser);
    boost::filesystem::copy_file(localRepositoryDir / "ssh/id_rsa.pub", opensshDirInBundle / "etc/userkeys/id_rsa.pub");
    sarus::common::setOwner(opensshDirInBundle / "etc/userkeys/id_rsa.pub", uidOfUser, gidOfUser);
    boost::filesystem::copy_file(localRepositoryDir / "ssh/id_rsa.pub", opensshDirInBundle / "etc/userkeys/authorized_keys");
    sarus::common::setOwner(opensshDirInBundle / "etc/userkeys/authorized_keys", uidOfUser, gidOfUser);
}

void SshHook::addSshdUserIfNecessary() const {
    auto passwd = sarus::common::PasswdDB{};
    passwd.read(rootfsDir / "etc/passwd");

    // check if user already exists
    for(const auto& entry : passwd.getEntries()) {
        if(entry.loginName == "sshd") {
            return;
        }
    }

    // create new entry in passwd DB
    auto ids = getFreeUserIdsInContainer();
    auto newEntry = sarus::common::PasswdDB::Entry{
        "sshd",
        "x",
        std::get<0>(ids),
        std::get<1>(ids),
        "",
        boost::filesystem::path{"/run/sshd"},
        boost::filesystem::path{"/usr/sbin/nologin"}
    };
    passwd.getEntries().push_back(newEntry);

    passwd.write(rootfsDir / "etc/passwd");
}

std::tuple<uid_t, gid_t> SshHook::getFreeUserIdsInContainer() const {
    std::unordered_set<uid_t> uids;
    std::unordered_set<gid_t> gids;

    auto passwd = sarus::common::PasswdDB{};
    passwd.read(rootfsDir / "passwd");

    for(const auto& entry : passwd.getEntries()) {
        uids.insert(entry.uid);
        gids.insert(entry.gid);
    }

    return std::tuple<uid_t, gid_t>{findFreeId(uids), findFreeId(gids)};
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
    auto status = sarus::common::forkExecWait({"/opt/sarus/openssh/sbin/sshd"}, rootfsDir);
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
