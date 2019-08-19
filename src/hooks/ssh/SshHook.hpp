/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_ssh_SshHook_hpp
#define sarus_hooks_ssh_SshHook_hpp

#include <fstream>
#include <cstdlib>
#include <tuple>
#include <algorithm>
#include <unordered_set>
#include <sys/types.h>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/Utility.hpp"
#include "common/SecurityChecks.hpp"
#include "runtime/mount_utilities.hpp"
#include "hooks/common/Utility.hpp"
#include "PasswdDB.hpp"


namespace sarus {
namespace hooks {
namespace ssh {

class SshHook {
public:
    void generateSshKeys() {
        localRepositoryDir = sarus::common::getEnvironmentVariable("SARUS_LOCAL_REPOSITORY_DIR");
        opensshDirInHost = sarus::common::getEnvironmentVariable("SARUS_OPENSSH_DIR");
        boost::filesystem::remove_all(localRepositoryDir / "ssh"); // remove previously existing keys (if any)
        sarus::common::createFoldersIfNecessary(localRepositoryDir / "ssh");
        sshKeygen(localRepositoryDir / "ssh/ssh_host_rsa_key");
        sshKeygen(localRepositoryDir / "ssh/id_rsa");
    }

    void checkLocalRepositoryHasSshKeys() {
        localRepositoryDir = sarus::common::getEnvironmentVariable("SARUS_LOCAL_REPOSITORY_DIR");
        auto expectedKeyFiles = std::vector<std::string>{"ssh_host_rsa_key", "ssh_host_rsa_key.pub", "id_rsa", "id_rsa.pub"};
        for(const auto& file : expectedKeyFiles) {
            auto path = localRepositoryDir / "ssh" / file;
            if(!boost::filesystem::exists(path)) {
                auto message = boost::format("Local repository doesn't contain SSH key %s as expected") % path;
                SARUS_THROW_ERROR(message.str());
            }
        }
    }

    void startSshd() {
        localRepositoryBaseDir = sarus::common::getEnvironmentVariable("SARUS_LOCAL_REPOSITORY_BASE_DIR");
        opensshDirInHost = sarus::common::getEnvironmentVariable("SARUS_OPENSSH_DIR");
        checkThatOpenSshIsUntamperable();
        std::tie(bundleDir, pidOfContainer) = hooks::common::utility::parseStateOfContainerFromStdin();
        hooks::common::utility::enterNamespacesOfProcess(pidOfContainer);
        parseConfigJSONOfBundle();
        if(!isHookEnabled) {
            return;
        }
        localRepositoryDir = sarus::common::getLocalRepositoryDirectory(localRepositoryBaseDir, uidOfUser);
        sarus::common::copyFolder(opensshDirInHost, opensshDirInBundle);
        copyKeysIntoBundle();
        sarus::common::createFoldersIfNecessary(rootfsDir / "tmp/var/empty"); // sshd's chroot folder
        addSshdUserIfNecessary();
        patchPasswdIfNecessary();
        startSshdInContainer();
        bindMountSshBinary();
    }

private:
    void parseConfigJSONOfBundle() {
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

    void sshKeygen(const boost::filesystem::path& outputFile) const {
        auto command = boost::format("%s/bin/ssh-keygen -t rsa -f %s -N \"\"")
            % opensshDirInHost
            % outputFile;
        sarus::common::executeCommand(command.str());
    }

    void checkThatOpenSshIsUntamperable() const {
        boost::filesystem::path sarusInstallationPrefixDir = sarus::common::getEnvironmentVariable("SARUS_PREFIX_DIR");
        auto configFile =  sarusInstallationPrefixDir / "etc/sarus.json";
        auto configSchemaFile = sarusInstallationPrefixDir / "etc/sarus.schema.json";
        auto config = std::make_shared<sarus::common::Config>();
        config->initializeJson(config, configFile, configSchemaFile);
        sarus::common::SecurityChecks{std::move(config)}.checkThatPathIsUntamperable(opensshDirInHost);
    }

    void copyKeysIntoBundle() const {
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

    void addSshdUserIfNecessary() const {
        auto passwd = PasswdDB{};
        passwd.read(rootfsDir / "etc/passwd");

        // check if user already exists
        for(const auto& entry : passwd.getEntries()) {
            if(entry.loginName == "sshd") {
                return;
            }
        }

        // create new entry in passwd DB
        auto ids = getFreeUserIdsInContainer();
        auto newEntry = PasswdDB::Entry{
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

    std::tuple<uid_t, gid_t> getFreeUserIdsInContainer() const {
        std::unordered_set<uid_t> uids;
        std::unordered_set<gid_t> gids;

        auto passwd = PasswdDB{};
        passwd.read(rootfsDir / "passwd");

        for(const auto& entry : passwd.getEntries()) {
            uids.insert(entry.uid);
            gids.insert(entry.gid);
        }

        return std::tuple<uid_t, gid_t>{findFreeId(uids), findFreeId(gids)};
    }

    template<class T>
    T findFreeId(const std::unordered_set<T>& ids) const {
        for(T id=0; id<std::numeric_limits<T>::max(); ++id) {
            if(ids.find(id) == ids.cend()) {
                return id;
            }
        }
        SARUS_THROW_ERROR("Failed to find free ID");
    }

    void bindMountSshBinary() const {
        sarus::common::createFileIfNecessary(rootfsDir / "usr/bin/ssh");
        runtime::bindMount(opensshDirInBundle / "bin/ssh", rootfsDir / "usr/bin/ssh");
    }

    void patchPasswdIfNecessary() const {
        auto passwd = PasswdDB{};
        passwd.read(rootfsDir / "etc/passwd");
        for(auto& entry : passwd.getEntries()) {
            if( entry.userCommandInterpreter
                && !boost::filesystem::exists(rootfsDir / *entry.userCommandInterpreter)) {
                entry.userCommandInterpreter = "/bin/sh";
            }
        }
        passwd.write(rootfsDir / "etc/passwd");
    }

    void startSshdInContainer() const {
        sarus::common::forkExecWait({"/opt/sarus/openssh/sbin/sshd"}, rootfsDir);
    }

private:
    bool isHookEnabled = false;
    boost::filesystem::path localRepositoryBaseDir;
    boost::filesystem::path localRepositoryDir;
    boost::filesystem::path opensshDirInHost;
    boost::filesystem::path opensshDirInBundle;
    boost::filesystem::path bundleDir;
    boost::filesystem::path rootfsDir;
    pid_t pidOfContainer;
    uid_t uidOfUser;
    gid_t gidOfUser;
};

}}} // namespace

#endif
