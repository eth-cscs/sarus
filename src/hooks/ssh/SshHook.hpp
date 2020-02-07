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
#include <common/Config.hpp>


namespace sarus {
namespace hooks {
namespace ssh {

class SshHook {
public:
    SshHook(std::shared_ptr<sarus::common::Config> config);
    void generateSshKeys(bool overwriteSshKeysIfExist);
    void checkLocalRepositoryHasSshKeys();
    void startSshd();

private:
    void parseConfigJSONOfBundle();
    bool localRepositoryHasSshKeys() const;
    boost::filesystem::path getKeysDirInLocalRepository() const;
    void sshKeygen(const boost::filesystem::path& outputFile) const;
    void checkThatOpenSshIsUntamperable() const;
    void copyKeysIntoBundle() const;
    void bindMountSshBinary() const;
    void patchPasswdIfNecessary() const;
    void startSshdInContainer() const;
    void log(const boost::format& message, sarus::common::LogLevel level) const;
    void log(const std::string& message, sarus::common::LogLevel level) const;

private:
    bool isHookEnabled = false;
    boost::filesystem::path localRepositoryDir;
    boost::filesystem::path opensshDirInHost;
    boost::filesystem::path opensshDirInBundle;
    boost::filesystem::path bundleDir;
    boost::filesystem::path rootfsDir;
    pid_t pidOfContainer;
    uid_t uidOfUser;
    gid_t gidOfUser;
    std::shared_ptr<sarus::common::Config> config;
};

}}} // namespace

#endif
