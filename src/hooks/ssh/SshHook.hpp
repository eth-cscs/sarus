/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_ssh_SshHook_hpp
#define sarus_hooks_ssh_SshHook_hpp

#include <tuple>
#include <memory>
#include <unordered_set>
#include <sys/types.h>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include <common/Error.hpp>
#include <common/Logger.hpp>

namespace sarus {
namespace hooks {
namespace ssh {

class SshHook {
public:
    void generateSshKeys(bool overwriteSshKeysIfExist);
    void checkUserHasSshKeys();
    void startSshDaemon();

private:
    void parseConfigJSONOfBundle();
    bool userHasSshKeys() const;
    std::string getUsername(uid_t) const;
    boost::filesystem::path getSshKeysDirInHost(const std::string& username) const;
    void sshKeygen(const boost::filesystem::path& outputFile) const;
    void generateAuthorizedKeys(const boost::filesystem::path& userKeyFile,
                                const boost::filesystem::path& authorizedKeysFile) const;
    void copyDropbearIntoContainer() const;
    void setupSshKeysDirInContainer() const;
    void copySshKeysIntoContainer() const;
    void createSshExecutableInContainer() const;
    void patchPasswdIfNecessary() const;
    void startSshDaemonInContainer() const;
    void log(const boost::format& message, sarus::common::LogLevel level) const;
    void log(const std::string& message, sarus::common::LogLevel level) const;

private:
    bool isHookEnabled = false;
    std::string username;
    boost::filesystem::path sshKeysDirInHost;
    boost::filesystem::path sshKeysDirInContainer;
    boost::filesystem::path dropbearDirInHost;
    boost::filesystem::path dropbearDirInContainer;
    boost::filesystem::path bundleDir;
    boost::filesystem::path rootfsDir;
    pid_t pidOfContainer;
    uid_t uidOfUser;
    gid_t gidOfUser;
    std::uint16_t serverPort;
};

}}} // namespace

#endif
