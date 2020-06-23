/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <memory>

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <boost/regex.hpp>

#include "common/Config.hpp"
#include "common/Logger.hpp"
#include "common/PathRAII.hpp"
#include "common/Utility.hpp"
#include "hooks/common/Utility.hpp"
#include "runtime/mount_utilities.hpp"
#include "hooks/ssh/SshHook.hpp"
#include "test_utility/Misc.hpp"
#include "test_utility/config.hpp"
#include "test_utility/filesystem.hpp"
#include "test_utility/OCIHooks.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace rj = rapidjson;

namespace sarus {
namespace hooks {
namespace ssh {
namespace test {

class Helper {
public:
    Helper() {
        configRAII.config->userIdentity.uid = std::get<0>(idsOfUser);
        configRAII.config->userIdentity.gid = std::get<1>(idsOfUser);
    }

    void setupTestEnvironment() {
        // host test environment
        sarus::common::createFoldersIfNecessary(sshKeysDir, std::get<0>(idsOfUser), std::get<1>(idsOfUser));

        sarus::common::createFoldersIfNecessary(opensshDirInHost.getPath());
        boost::format extractArchiveCommand = boost::format("tar xf %s -C %s --strip-components=1")
            % sarus::common::Config::BuildTime{}.openSshArchive
            % opensshDirInHost.getPath();
        sarus::common::executeCommand(extractArchiveCommand.str());

        sarus::common::setEnvironmentVariable("HOOK_BASE_DIR=" + sshKeysBaseDir.string());
        sarus::common::setEnvironmentVariable("PASSWD_FILE=" + passwdFile.string());
        sarus::common::setEnvironmentVariable("OPENSSH_DIR=" + opensshDirInHost.getPath().string());

        // bundle test environment
        auto doc = test_utility::ocihooks::createBaseConfigJSON(rootfsDir, idsOfUser);
        sarus::common::writeJSON(doc, bundleDir / "config.json");

        for(const auto& folder : rootfsFolders) {
            auto lowerDir = "/" / folder;
            auto upperDir = bundleDir / "upper-dirs" / folder;
            auto workDir = bundleDir / "work-dirs" / folder;
            auto mergedDir = rootfsDir / folder;

            sarus::common::createFoldersIfNecessary(upperDir);
            sarus::common::createFoldersIfNecessary(workDir);
            sarus::common::createFoldersIfNecessary(mergedDir);

            runtime::mountOverlayfs(lowerDir, upperDir, workDir, mergedDir);
        }
    }

    void writeContainerStateToStdin() const {
        test_utility::ocihooks::writeContainerStateToStdin(bundleDir);
    }

    void cleanupTestEnvironment() const {
        // bundle test environment
        CHECK(umount((rootfsDir / "usr/bin/ssh").c_str()) == 0);

        for(const auto& folder : rootfsFolders) {
            CHECK(umount2((rootfsDir / folder).c_str(), MNT_FORCE | MNT_DETACH) == 0);
            boost::filesystem::remove_all(rootfsDir / folder);
        }
    }

    void setUserIds() const {
        if(setresuid(std::get<0>(idsOfUser), std::get<0>(idsOfUser), std::get<0>(idsOfRoot)) != 0) {
            auto message = boost::format("Failed to set uid %d: %s") % std::get<0>(idsOfUser) % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }
    }

    void setRootIds() const {
        if(setresuid(std::get<0>(idsOfRoot), std::get<0>(idsOfRoot), std::get<0>(idsOfRoot)) != 0) {
            auto message = boost::format("Failed to set uid %d: %s") % std::get<0>(idsOfRoot) % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }
    }

    void checkLocalRepositoryHasSshKeys() const {
        CHECK(boost::filesystem::exists(sshKeysDir / "ssh_host_rsa_key"));
        CHECK(boost::filesystem::exists(sshKeysDir / "ssh_host_rsa_key.pub"));
        CHECK(boost::filesystem::exists(sshKeysDir / "id_rsa"));
        CHECK(boost::filesystem::exists(sshKeysDir / "id_rsa.pub"));
    }

    void checkContainerHasServerKeys() const {
        CHECK(boost::filesystem::exists(opensshDirInContainer / "etc/ssh_host_rsa_key"));
        CHECK(sarus::common::getOwner(opensshDirInContainer / "etc/ssh_host_rsa_key") == idsOfUser)
        CHECK(boost::filesystem::exists(opensshDirInContainer / "etc/ssh_host_rsa_key.pub"));
        CHECK(sarus::common::getOwner(opensshDirInContainer / "etc/ssh_host_rsa_key.pub") == idsOfUser);
    }

    void checkContainerHasClientKeys() const {
        CHECK(boost::filesystem::exists(opensshDirInContainer / "etc/userkeys/id_rsa"));
        CHECK(sarus::common::getOwner(opensshDirInContainer / "etc/userkeys/id_rsa") == idsOfUser);
        CHECK(boost::filesystem::exists(opensshDirInContainer / "etc/userkeys/id_rsa.pub"));
        CHECK(sarus::common::getOwner(opensshDirInContainer / "etc/userkeys/id_rsa.pub") == idsOfUser);
        CHECK(boost::filesystem::exists(opensshDirInContainer / "etc/userkeys/authorized_keys"));
        CHECK(sarus::common::getOwner(opensshDirInContainer / "etc/userkeys/authorized_keys") == idsOfUser);
    }

    void checkContainerHasChrootFolderForSshd() const {
        CHECK(boost::filesystem::exists(rootfsDir / "tmp/var/empty"));
        CHECK(sarus::common::getOwner(rootfsDir / "tmp/var/empty") == idsOfRoot);
    }

    bool isSshdRunningInContainer() const {
        auto out = sarus::common::executeCommand("ps ax -o pid,args");
        std::stringstream ss{out};
        std::string line;

        boost::smatch matches;
        boost::regex pattern("^ *[0-9]+ +/opt/sarus/openssh/sbin/sshd.*$");

        while(std::getline(ss, line)) {
            if(boost::regex_match(line, matches, pattern)) {
                return true;
            }
        }
        return false;
    }

    void checkContainerHasSshBinary() const {
        //check container's /usr/bin/ssh is /opt/sarus/openssh/bin/ssh
        CHECK(test_utility::filesystem::isSameBindMountedFile(rootfsDir / "usr/bin/ssh", opensshDirInContainer / "bin/ssh"));
    }

private:
    std::tuple<uid_t, gid_t> idsOfRoot{0, 0};
    std::tuple<uid_t, gid_t> idsOfUser = test_utility::misc::getNonRootUserIds();

    test_utility::config::ConfigRAII configRAII = test_utility::config::makeConfig();
    boost::filesystem::path prefixDir = configRAII.config->json["prefixDir"].GetString();
    boost::filesystem::path passwdFile = prefixDir / "etc/passwd";
    boost::filesystem::path bundleDir = configRAII.config->json["OCIBundleDir"].GetString();
    boost::filesystem::path rootfsDir = bundleDir / configRAII.config->json["rootfsFolder"].GetString();
    boost::filesystem::path sshKeysBaseDir = configRAII.config->json["localRepositoryBaseDir"].GetString();
    std::string username = sarus::common::PasswdDB{passwdFile}.getUsername(std::get<0>(idsOfUser));
    boost::filesystem::path sshKeysDir = sshKeysBaseDir / username / ".oci-hooks/ssh/keys";
    sarus::common::PathRAII opensshDirInHost = sarus::common::PathRAII{boost::filesystem::absolute(
                                               sarus::common::makeUniquePathWithRandomSuffix("./sarus-test-opensshstatic"))};
    boost::filesystem::path opensshDirInContainer = rootfsDir / "opt/sarus/openssh";
    std::vector<boost::filesystem::path> rootfsFolders = {"etc", "dev", "bin", "sbin", "usr", "lib", "lib64"}; // necessary to chroot into rootfs
};

TEST_GROUP(SSHHookTestGroup) {
};

TEST(SSHHookTestGroup, testSshHook) {
    auto helper = Helper{};

    helper.setRootIds();
    helper.setupTestEnvironment();

    // generate + check SSH keys in local repository
    helper.setUserIds(); // keygen is executed with user privileges
    SshHook{}.generateSshKeys(true);
    SshHook{}.checkUserHasSshKeys();
    helper.setRootIds();
    helper.checkLocalRepositoryHasSshKeys();

    // start sshd
    helper.writeContainerStateToStdin();
    SshHook{}.startSshd();
    helper.checkContainerHasServerKeys();
    helper.checkContainerHasClientKeys();
    helper.checkContainerHasChrootFolderForSshd();
    CHECK(helper.isSshdRunningInContainer());
    helper.checkContainerHasSshBinary();

    helper.cleanupTestEnvironment();
}

}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
