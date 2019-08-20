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

#include <tuple>
#include <memory>
#include <unordered_set>
#include <sys/types.h>
#include <boost/filesystem.hpp>

#include <common/Error.hpp>
#include <common/Config.hpp>


namespace sarus {
namespace hooks {
namespace ssh {

class SshHook {
public:
    void generateSshKeys();
    void checkLocalRepositoryHasSshKeys();
    void startSshd();

private:
    void parseConfigJSONOfBundle();
    std::shared_ptr<sarus::common::Config> parseConfigJSONOfSarus(uid_t uidOfUser, gid_t gidOfUser) const;
    void sshKeygen(const boost::filesystem::path& outputFile) const;
    void checkThatOpenSshIsUntamperable() const;
    void copyKeysIntoBundle() const;
    void addSshdUserIfNecessary() const;
    std::tuple<uid_t, gid_t> getFreeUserIdsInContainer() const;

    template<class T>
    T findFreeId(const std::unordered_set<T>& ids) const {
        for(T id=0; id<std::numeric_limits<T>::max(); ++id) {
            if(ids.find(id) == ids.cend()) {
                return id;
            }
        }
        SARUS_THROW_ERROR("Failed to find free ID");
    }

    void bindMountSshBinary() const;
    void patchPasswdIfNecessary() const;
    void startSshdInContainer() const;

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
};

}}} // namespace

#endif
