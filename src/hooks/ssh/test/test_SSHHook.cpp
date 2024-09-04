/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <chrono>
#include <memory>
#include <signal.h>
#include <thread>
#include <sys/mount.h>
#include <sys/types.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/regex.hpp>

#include "common/Config.hpp"
#include "libsarus/PathRAII.hpp"
#include "libsarus/Utility.hpp"
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

    ~Helper() {
        // get root privileges in case that the test failure
        // occurred while we had non-root privileges
        setRootIds();

        // undo mounts in rootfs
        for(const auto& folder : rootfsFolders) {
            umount2((rootfsDir / folder).c_str(), MNT_FORCE | MNT_DETACH);
        }

        // undo overlayfs mount in ~/.ssh
        umount2((expectedHomeDirInContainer / ".ssh").c_str(), MNT_FORCE | MNT_DETACH);

        // undo tmpfs mount on bundle directory
        umount2((bundleDir).c_str(), MNT_FORCE | MNT_DETACH);

        // kill SSH daemon
        auto pid = getSshDaemonPid();
        if(pid) {
            kill(*pid, SIGTERM);
        }

        // NOTE: the test directories are automatically removed by the PathRAII objects
    }

    void setupTestEnvironment() {
        // create tmpfs filesystem to allow overlay mounts for rootfs (performed below)
        // to succeed also when testing inside a Docker container
        libsarus::filesystem::createFoldersIfNecessary(bundleDir);
        if(mount(NULL, bundleDir.c_str(), "tmpfs", MS_NOSUID|MS_NODEV, NULL) != 0) {
            auto message = boost::format("Failed to setup tmpfs filesystem on %s: %s")
                % bundleDir
                % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }

        libsarus::filesystem::createFoldersIfNecessary(homeDirInHost,
                                                std::get<0>(idsOfUser),
                                                std::get<1>(idsOfUser));

        libsarus::filesystem::createFoldersIfNecessary(expectedHomeDirInContainer,
                                                std::get<0>(idsOfUser),
                                                std::get<1>(idsOfUser));

        // host's dropbear installation
        libsarus::filesystem::createFoldersIfNecessary(dropbearDirInHost.getPath() / "bin");
        boost::format setupDropbearCommand = boost::format{
            "cp %1% %2%/bin/dropbearmulti"
            " && ln -s %2%/bin/dropbearmulti %2%/bin/dbclient"
            " && ln -s %2%/bin/dropbearmulti %2%/bin/dropbear"
            " && ln -s %2%/bin/dropbearmulti %2%/bin/dropbearkey"
        } % sarus::common::Config::BuildTime{}.dropbearmultiBuildArtifact.string()
          % dropbearDirInHost.getPath().string();
        libsarus::process::executeCommand(setupDropbearCommand.str());

        // hook's environment variables
        libsarus::environment::setVariable("HOOK_BASE_DIR", sshKeysBaseDir.string());
        libsarus::environment::setVariable("PASSWD_FILE", passwdFile.string());
        libsarus::environment::setVariable("DROPBEAR_DIR", dropbearDirInHost.getPath().string());
        libsarus::environment::setVariable("SERVER_PORT_DEFAULT", std::to_string(serverPortDefault));

        if (!userSshKeyPath.empty()) {
            std::ofstream userSshKeyFile{userSshKeyPath.string()};
            userSshKeyFile << userSshKey;
        }

        createConfigJSON();

        // rootfs
        for(const auto& folder : rootfsFolders) {
            auto lowerDir = "/" / folder;
            auto upperDir = bundleDir / "upper-dirs" / folder;
            auto workDir = bundleDir / "work-dirs" / folder;
            auto mergedDir = rootfsDir / folder;

            libsarus::filesystem::createFoldersIfNecessary(upperDir);
            libsarus::filesystem::createFoldersIfNecessary(workDir);
            libsarus::filesystem::createFoldersIfNecessary(mergedDir);

            libsarus::mount::mountOverlayfs(lowerDir, upperDir, workDir, mergedDir);
        }

        // set requested home dir in /etc/passwd
        auto passwd = libsarus::PasswdDB{rootfsDir / "etc/passwd"};
        for (auto& entry : passwd.getEntries()) {
            if(entry.uid == std::get<0>(idsOfUser)) {
                entry.userHomeDirectory = "/" / boost::filesystem::relative(homeDirInContainerPasswd, rootfsDir);
            }
        }
        passwd.write(rootfsDir / "etc/passwd");

        // Ensure parent path for container pidfile is writable by user
        if (boost::filesystem::exists(getDropbearPidFileInContainerAbsolute().parent_path())) {
            auto addOthersWritePerms = boost::filesystem::perms::add_perms | boost::filesystem::perms::others_write;
            boost::filesystem::permissions(getDropbearPidFileInContainerAbsolute().parent_path(), addOthersWritePerms);
        }
    }

    void createConfigJSON() {
        namespace rj = rapidjson;
        auto doc = test_utility::ocihooks::createBaseConfigJSON(rootfsDir, idsOfUser);
        auto& allocator = doc.GetAllocator();
        for (const auto& var : environmentVariablesInContainer) {
            doc["process"]["env"].PushBack(rj::Value{var.c_str(), allocator}, allocator);
        }
        rapidjson::Value extraAnnotations(rapidjson::kObjectType);
        if (boost::filesystem::exists(userSshKeyPath)) {
            extraAnnotations.AddMember(
                "com.hooks.ssh.authorize_ssh_key",
                rj::Value{userSshKeyPath.c_str(), allocator},
                allocator);
        }
        if (!dropbearPidFileInContainerRelative.empty()) {
          extraAnnotations.AddMember(
              "com.hooks.ssh.pidfile_container",
              rj::Value{dropbearPidFileInContainerRelative.c_str(),
                        allocator},
              allocator);
        }
        if (!dropbearPidFileInHost.getPath().empty()) {
          extraAnnotations.AddMember(
              "com.hooks.ssh.pidfile_host",
              rj::Value{dropbearPidFileInHost.getPath().c_str(), allocator},
              allocator);
        }
        if (serverPort > 0) {
          extraAnnotations.AddMember(
              "com.hooks.ssh.port",
              rj::Value{std::to_string(serverPort).c_str(), allocator},
              allocator);
        }
        if (!doc.HasMember("annotations")) {
          doc.AddMember("annotations", extraAnnotations, allocator);
        } else {
          doc["annotations"] = extraAnnotations;
        }

        libsarus::json::write(doc, bundleDir / "config.json");
    }

    void writeContainerStateToStdin() const {
        test_utility::ocihooks::writeContainerStateToStdin(bundleDir);
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

    void setExpectedHomeDirInContainer(const boost::filesystem::path& path) {
        expectedHomeDirInContainer = rootfsDir / path;
    }

    void setHomeDirInContainerPasswd(const boost::filesystem::path& path) {
        homeDirInContainerPasswd = rootfsDir / path;
    }

    void setEnvironmentVariableInContainer(const std::string& variable) {
        environmentVariablesInContainer.push_back(variable);
    }

    void enableUserSshKeyPath() {
        userSshKeyPath = homeDirInHost / "user_key.pub";
    }

    void checkHostHasSshKeys() const {
        CHECK(boost::filesystem::exists(sshKeysDirInHost / "dropbear_ecdsa_host_key"));
        CHECK(boost::filesystem::exists(sshKeysDirInHost / "id_dropbear"));
        CHECK(boost::filesystem::exists(sshKeysDirInHost / "authorized_keys"));
    }

    void checkContainerHasServerKeys() const {
        CHECK(boost::filesystem::exists(expectedHomeDirInContainer / ".ssh/dropbear_ecdsa_host_key"));
        CHECK(libsarus::filesystem::getOwner(expectedHomeDirInContainer / ".ssh/dropbear_ecdsa_host_key") == idsOfUser);
    }

    void checkContainerHasClientKeys() const {
        auto userKeyFile = expectedHomeDirInContainer / ".ssh/id_dropbear";
        auto authorizedKeysFile = expectedHomeDirInContainer / ".ssh/authorized_keys";

        CHECK(boost::filesystem::exists(userKeyFile));
        CHECK(libsarus::filesystem::getOwner(userKeyFile) == idsOfUser);
        CHECK(boost::filesystem::exists(authorizedKeysFile));
        CHECK(libsarus::filesystem::getOwner(authorizedKeysFile) == idsOfUser);

        auto expectedAuthKeysPermissions =
            boost::filesystem::owner_read | boost::filesystem::owner_write |
            boost::filesystem::group_read |
            boost::filesystem::others_read;
        auto status = boost::filesystem::status(authorizedKeysFile);
        CHECK(status.permissions() == expectedAuthKeysPermissions);
    }

    boost::optional<pid_t> getSshDaemonPid() const {
        auto out = libsarus::process::executeCommand("ps ax -o pid,args");
        std::stringstream ss{out};
        std::string line;

        boost::smatch matches;
        boost::regex pattern("^ *([0-9]+) +/opt/oci-hooks/ssh/dropbear/bin/dropbear.*$");

        while(std::getline(ss, line)) {
            if(boost::regex_match(line, matches, pattern)) {
                return std::stoi(matches[1]);
            }
        }
        return {};
    }

    boost::optional<std::uint16_t> getSshDaemonPort() const {
        auto out = libsarus::process::executeCommand("ps ax -o args");
        std::stringstream ss{out};
        std::string line;

        boost::smatch matches;
        boost::regex pattern("^ */opt/oci-hooks/ssh/dropbear/bin/dropbear.*-p ([0-9]+).*$");

        while(std::getline(ss, line)) {
            if(boost::regex_match(line, matches, pattern)) {
                return std::stoi(matches[1]);
            }
        }
        return {};
    }

    void checkDefaultSshDaemonPort() const {
        CHECK(getSshDaemonPort() == serverPortDefault);
    }

    void checkContainerHasSshBinary() const {
        auto targetFile = boost::filesystem::path(rootfsDir / "usr/bin/ssh");
        CHECK(boost::filesystem::exists(targetFile));

        auto expectedScript = boost::format{
            "#!/bin/sh\n"
            "/opt/oci-hooks/ssh/dropbear/bin/dbclient -y -p %s $*\n"
        } % (serverPort > 0 ? serverPort : serverPortDefault);
        auto actualScript = libsarus::filesystem::readFile(targetFile);
        CHECK_EQUAL(actualScript, expectedScript.str());

        auto expectedPermissions =
            boost::filesystem::owner_all |
            boost::filesystem::group_read | boost::filesystem::group_exe |
            boost::filesystem::others_read | boost::filesystem::others_exe;
        auto status = boost::filesystem::status(targetFile);
        CHECK(status.permissions() == expectedPermissions);
    }

    void checkContainerHasEnvironmentFile() const {
        auto targetFile = boost::filesystem::path(dropbearDirInContainer / "environment");
        CHECK(boost::filesystem::exists(targetFile));

        auto expectedMap = std::unordered_map<std::string, std::string>{};
        for (const auto& variable : environmentVariablesInContainer) {
            std::string key, value;
            std::tie(key, value) = libsarus::environment::parseVariable(variable);
            expectedMap[key] = value;
        }

        boost::filesystem::ifstream actualLines{targetFile};
        std::string actualLine;
        std::getline(actualLines, actualLine);
        CHECK_EQUAL(actualLine, std::string{"#!/bin/sh"});

        auto actualMap = std::unordered_map<std::string, std::string>{};
        // first line is the shebang, last line is empty, so to parse the variable definitions
        // we iterate from begin+1 to end-1
        while (std::getline(actualLines, actualLine)) {
            auto tokens = std::vector<std::string>{};
            boost::split(tokens, actualLine, boost::is_any_of(" "));
            CHECK_EQUAL(tokens[0], std::string{"export"});
            auto keyValue = std::vector<std::string>{};
            boost::split(keyValue, tokens[1], boost::is_any_of("="));
            std::string key = keyValue[0];
            std::string value = keyValue[1].substr(1, keyValue[1].size()-2); // remove first and last double-quotes
            actualMap[key] = value;
        }
        CHECK(actualMap == expectedMap);

        auto expectedPermissions =
            boost::filesystem::owner_all |
            boost::filesystem::group_read |
            boost::filesystem::others_read;
        auto status = boost::filesystem::status(targetFile);
        CHECK(status.permissions() == expectedPermissions);
    }

    void checkContainerHasEtcProfileModule() const {
        auto targetFile = boost::filesystem::path(rootfsDir / "etc/profile.d/ssh-hook.sh");
        CHECK(boost::filesystem::exists(targetFile));

        auto expectedScript = std::string(
                "#!/bin/sh\n"
                "if [ \"$SSH_CONNECTION\" ]; then\n"
                "    . /opt/oci-hooks/ssh/dropbear/environment\n"
                "fi\n");
        auto actualScript = libsarus::filesystem::readFile(targetFile);
        CHECK_EQUAL(actualScript, expectedScript);

        auto expectedPermissions =
                boost::filesystem::owner_read | boost::filesystem::owner_write |
                boost::filesystem::group_read |
                boost::filesystem::others_read;
        auto status = boost::filesystem::status(targetFile);
        CHECK(status.permissions() == expectedPermissions);
    }

    bool isUserSshKeyAuthorized() {
        std::ifstream authorizedKeysFile{(expectedHomeDirInContainer / ".ssh/authorized_keys").string()};
        if(!authorizedKeysFile.is_open()) {
            return false;
        }
        std::string line;
        while(getline(authorizedKeysFile, line)) {
            if (line.find(userSshKey, 0) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    const boost::filesystem::path getDropbearPidFileInContainerAbsolute() const {
        return rootfsDir / libsarus::filesystem::realpathWithinRootfs(rootfsDir, dropbearPidFileInContainerRelative);
    }

    const boost::filesystem::path getDropbearPidFileInHost() const {
        return dropbearPidFileInHost.getPath();
    }

    void setDropbearPidFileInContainer(const std::string& PidFile) {
        dropbearPidFileInContainerRelative = PidFile;
    }

    void setDropbearPidFileInHost(const std::string& PidFile) {
        dropbearPidFileInHost = libsarus::PathRAII{PidFile};
    }

    void setCustomServerPort(const std::uint16_t portNumber) {
        serverPort = portNumber;
    }

    bool containerMountsDotSsh() {
        auto overlayDir = bundleDir / "overlay";
        if  (!boost::filesystem::exists(overlayDir) || !boost::filesystem::is_directory(overlayDir)) {
            return false;
        }
        std::set<std::string> foundLayers;
        for(auto& layer : boost::make_iterator_range(boost::filesystem::directory_iterator(overlayDir), {})){
            foundLayers.insert(layer.path().filename().string());
        }

        return foundLayers == std::set<std::string>{"ssh-lower", "ssh-upper", "ssh-work"};
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
    std::string username = libsarus::PasswdDB{passwdFile}.getUsername(std::get<0>(idsOfUser));
    boost::filesystem::path homeDirInHost = sshKeysBaseDir / username;
    boost::filesystem::path expectedHomeDirInContainer = rootfsDir / "home" / username;
    boost::filesystem::path homeDirInContainerPasswd = expectedHomeDirInContainer;
    boost::filesystem::path sshKeysDirInHost = homeDirInHost / ".oci-hooks/ssh/keys";
    libsarus::PathRAII dropbearDirInHost = libsarus::PathRAII{boost::filesystem::absolute(
                                               libsarus::filesystem::makeUniquePathWithRandomSuffix("./hook-test-dropbeardir-in-host"))};
    boost::filesystem::path dropbearDirInContainer = rootfsDir / "opt/oci-hooks/ssh/dropbear";
    boost::filesystem::path dropbearPidFileInContainerRelative = "/var/run/dropbear/dropbear.pid";
    libsarus::PathRAII dropbearPidFileInHost = libsarus::PathRAII("");
    std::uint16_t serverPortDefault = 11111;
    std::uint16_t serverPort = 0;
    std::vector<boost::filesystem::path> rootfsFolders = {"etc", "dev", "bin", "sbin", "usr", "lib", "lib64"}; // necessary to chroot into rootfs
    std::vector<std::string> environmentVariablesInContainer;
    std::string userSshKey{"ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAvAIP2SI2ON23c6ZP1c7gQf17P25npZLgHSxfwqRKNWh27p user@test"};
    boost::filesystem::path userSshKeyPath;
};

TEST_GROUP(SSHHookTestGroup) {
};

TEST(SSHHookTestGroup, testSshHook) {
    Helper helper{};

    helper.setRootIds();
    helper.setupTestEnvironment();

    // generate + check SSH keys in local repository
    helper.setUserIds(); // keygen is executed with user privileges
    SshHook{}.generateSshKeys(true);
    SshHook{}.checkUserHasSshKeys();
    helper.setRootIds();
    helper.checkHostHasSshKeys();

    // start sshd
    helper.writeContainerStateToStdin();
    SshHook{}.startStopSshDaemon();
    helper.checkContainerHasClientKeys();
    helper.checkContainerHasServerKeys();
    CHECK(static_cast<bool>(helper.getSshDaemonPid()));
    helper.checkContainerHasSshBinary();
}

TEST(SSHHookTestGroup, testNonStandardHomeDir) {
    Helper helper{};

    helper.setRootIds();
    helper.setHomeDirInContainerPasswd("/users/test-home-dir");
    helper.setExpectedHomeDirInContainer("/users/test-home-dir");
    helper.setupTestEnvironment();

    // generate + check SSH keys in local repository
    helper.setUserIds(); // keygen is executed with user privileges
    SshHook{}.generateSshKeys(true);
    SshHook{}.checkUserHasSshKeys();
    helper.setRootIds();
    helper.checkHostHasSshKeys();

    // start sshd
    helper.writeContainerStateToStdin();
    SshHook{}.startStopSshDaemon();
    helper.checkContainerHasClientKeys();
    helper.checkContainerHasServerKeys();
    CHECK(static_cast<bool>(helper.getSshDaemonPid()));
    helper.checkContainerHasSshBinary();
}

TEST(SSHHookTestGroup, testSetEnvironmentOnLogin) {
    Helper helper{};

    helper.setRootIds();
    helper.setHomeDirInContainerPasswd("/users/test-home-dir");
    helper.setExpectedHomeDirInContainer("/users/test-home-dir");
    helper.setEnvironmentVariableInContainer("PATH=/bin:/usr/bin:/usr/local/bin:/sbin");
    helper.setEnvironmentVariableInContainer("TEST1=VariableTest1");
    helper.setEnvironmentVariableInContainer("TEST2=VariableTest2");
    helper.setupTestEnvironment();

    // generate + check SSH keys in local repository
    helper.setUserIds(); // keygen is executed with user privileges
    SshHook{}.generateSshKeys(true);
    SshHook{}.checkUserHasSshKeys();
    helper.setRootIds();
    helper.checkHostHasSshKeys();

    // start sshd
    helper.writeContainerStateToStdin();
    SshHook{}.startStopSshDaemon();
    helper.checkContainerHasEnvironmentFile();
    helper.checkContainerHasEtcProfileModule();
}

TEST(SSHHookTestGroup, testInjectKeyUsingAnnotations) {
    Helper helper{};

    helper.setRootIds();
    helper.enableUserSshKeyPath();
    helper.setupTestEnvironment();

    // generate + check SSH keys in local repository
    helper.setUserIds(); // keygen is executed with user privileges
    SshHook{}.generateSshKeys(true);

    helper.setRootIds();
    helper.checkHostHasSshKeys();

    // start sshd
    helper.writeContainerStateToStdin();
    SshHook{}.startStopSshDaemon();
    helper.checkContainerHasClientKeys();
    helper.checkContainerHasServerKeys();
    
    CHECK_TRUE(helper.isUserSshKeyAuthorized());
}

TEST(SSHHookTestGroup, testDefaultDropbearPidFiles) {
    Helper helper{};

    helper.setRootIds();
    helper.setupTestEnvironment();

    // generate + check SSH keys in local repository
    helper.setUserIds(); // keygen is executed with user privileges
    SshHook{}.generateSshKeys(true);

    helper.setRootIds();
    helper.checkHostHasSshKeys();

    // start sshd
    helper.writeContainerStateToStdin();
    SshHook{}.startStopSshDaemon();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    CHECK(boost::filesystem::is_regular_file(helper.getDropbearPidFileInContainerAbsolute()));
    CHECK_FALSE(boost::filesystem::exists(helper.getDropbearPidFileInHost()));
}

TEST(SSHHookTestGroup, testDropbearPidFileInHost) {
    Helper helper{};

    helper.setDropbearPidFileInHost((boost::filesystem::current_path() / "dropbear.pid").string());
    helper.setRootIds();
    helper.setupTestEnvironment();

    // generate + check SSH keys in local repository
    helper.setUserIds(); // keygen is executed with user privileges
    SshHook{}.generateSshKeys(true);

    helper.setRootIds();
    helper.checkHostHasSshKeys();

    // start sshd
    helper.writeContainerStateToStdin();
    SshHook{}.startStopSshDaemon();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto pidInContainer = libsarus::filesystem::readFile(helper.getDropbearPidFileInContainerAbsolute());
    auto pidInHost = libsarus::filesystem::readFile(helper.getDropbearPidFileInHost());
    CHECK(pidInContainer == pidInHost);
}

TEST(SSHHookTestGroup, testDropbearPidFilesInCustomPaths) {
    Helper helper{};

    helper.setDropbearPidFileInHost((boost::filesystem::current_path() / "dropbear.pid").string());
    helper.setDropbearPidFileInContainer("/etc/dropbear/dropbear.pid");
    helper.setRootIds();
    helper.setupTestEnvironment();

    // generate + check SSH keys in local repository
    helper.setUserIds(); // keygen is executed with user privileges
    SshHook{}.generateSshKeys(true);

    helper.setRootIds();
    helper.checkHostHasSshKeys();

    // start sshd
    helper.writeContainerStateToStdin();
    SshHook{}.startStopSshDaemon();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto pidInContainer = libsarus::filesystem::readFile(helper.getDropbearPidFileInContainerAbsolute());
    auto pidInHost = libsarus::filesystem::readFile(helper.getDropbearPidFileInHost());
    CHECK(pidInContainer == pidInHost);
}

TEST(SSHHookTestGroup, testDefaultMountsDotSshAsOverlayFs) {
    // libsarus::environment::setVariable("OVERLAY_MOUNT_HOME_SSH", "False");

    Helper helper{};

    helper.setRootIds();
    helper.setupTestEnvironment();

    // generate + check SSH keys in local repository
    helper.setUserIds(); // keygen is executed with user privileges
    SshHook{}.generateSshKeys(true);

    helper.setRootIds();

    // start sshd
    helper.writeContainerStateToStdin();
    SshHook{}.startStopSshDaemon();

    CHECK_TRUE(helper.containerMountsDotSsh());    
}

TEST(SSHHookTestGroup, testEnvVarDisableMountsDotSshAsOverlayFs) {
    libsarus::environment::setVariable("OVERLAY_MOUNT_HOME_SSH", "False");

    Helper helper{};

    helper.setRootIds();
    helper.setupTestEnvironment();

    // generate + check SSH keys in local repository
    helper.setUserIds(); // keygen is executed with user privileges
    SshHook{}.generateSshKeys(true);

    helper.setRootIds();

    // start sshd
    helper.writeContainerStateToStdin();
    SshHook{}.startStopSshDaemon();

    CHECK_FALSE(helper.containerMountsDotSsh());    
    libsarus::environment::setVariable("OVERLAY_MOUNT_HOME_SSH", "");
}

TEST(SSHHookTestGroup, testDefaultServerPort) {
    Helper helper{};

    helper.setRootIds();
    helper.setupTestEnvironment();

    // generate + check SSH keys in local repository
    helper.setUserIds(); // keygen is executed with user privileges
    SshHook{}.generateSshKeys(true);
    helper.setRootIds();
    helper.checkHostHasSshKeys();

    // start sshd
    helper.writeContainerStateToStdin();
    SshHook{}.startStopSshDaemon();
    helper.checkDefaultSshDaemonPort();
    helper.checkContainerHasSshBinary();
}

TEST(SSHHookTestGroup, testDefaultServerPortOverridesDeprecatedVar) {
    std::uint16_t expectedPort = 29476;
    Helper helper{};

    helper.setRootIds();
    helper.setupTestEnvironment(); // "SERVER_PORT_DEFAULT" is set here
    libsarus::environment::setVariable("SERVER_PORT", std::to_string(expectedPort));

    // generate + check SSH keys in local repository
    helper.setUserIds(); // keygen is executed with user privileges
    SshHook{}.generateSshKeys(true);
    helper.setRootIds();
    helper.checkHostHasSshKeys();

    // start sshd
    helper.writeContainerStateToStdin();
    SshHook{}.startStopSshDaemon();
    helper.checkDefaultSshDaemonPort();
}

TEST(SSHHookTestGroup, testDeprecatedServerPort) {
    std::uint16_t expectedPort = 44184;
    Helper helper{};

    helper.setRootIds();
    helper.setupTestEnvironment(); // "SERVER_PORT_DEFAULT" is set here
    libsarus::environment::setVariable("SERVER_PORT", std::to_string(expectedPort));
    unsetenv("SERVER_PORT_DEFAULT");

    // generate + check SSH keys in local repository
    helper.setUserIds(); // keygen is executed with user privileges
    SshHook{}.generateSshKeys(true);
    helper.setRootIds();
    helper.checkHostHasSshKeys();

    // start sshd
    helper.writeContainerStateToStdin();
    SshHook{}.startStopSshDaemon();
    CHECK(helper.getSshDaemonPort() == expectedPort);
}

TEST(SSHHookTestGroup, testCustomServerPort) {
    std::uint16_t expectedPort = 57864;
    Helper helper{};

    helper.setRootIds();
    helper.setCustomServerPort(expectedPort);
    helper.setupTestEnvironment();

    // generate + check SSH keys in local repository
    helper.setUserIds(); // keygen is executed with user privileges
    SshHook{}.generateSshKeys(true);
    helper.setRootIds();
    helper.checkHostHasSshKeys();

    // start sshd
    helper.writeContainerStateToStdin();
    SshHook{}.startStopSshDaemon();
    CHECK(helper.getSshDaemonPort() == expectedPort);
    helper.checkContainerHasSshBinary();
}

}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
