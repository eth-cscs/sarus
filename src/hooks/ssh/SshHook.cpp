/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "SshHook.hpp"

#include <chrono>
#include <thread>
#include <fstream>
#include <signal.h>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>

#include "libsarus/Error.hpp"
#include "libsarus/Utility.hpp"
#include "libsarus/Lockfile.hpp"
#include "libsarus/PasswdDB.hpp"


namespace sarus {
namespace hooks {
namespace ssh {

static bool eventuallyJoinNamespaces(const pid_t pidOfContainer) {
    bool joinNamespaces{true};
    try {
        auto envJoinNamespaces = libsarus::environment::getVariable("JOIN_NAMESPACES");
        joinNamespaces = (boost::algorithm::to_upper_copy(envJoinNamespaces) == std::string("TRUE"));
    } catch (libsarus::Error&) {}
    return joinNamespaces;
}

static std::uint16_t getServerPortFromEnv() {
    std::uint16_t serverPort;
    try {
        serverPort = std::stoi(libsarus::environment::getVariable("SERVER_PORT"));
    } catch (libsarus::Error&) {}
    try {
        serverPort = std::stoi(libsarus::environment::getVariable("SERVER_PORT_DEFAULT"));
    } catch (libsarus::Error& e) {
        if (serverPort == 0) {
            SARUS_RETHROW_ERROR(e, "At least one of the environment variables SERVER_PORT_DEFAULT (preferred)"
                                   " or SERVER_PORT (deprecated) must be defined.")
        }
    }
    return serverPort;
}

void SshHook::generateSshKeys(bool overwriteSshKeysIfExist) {
    log("Generating SSH keys", libsarus::LogLevel::INFO);

    uidOfUser = getuid(); // keygen command is executed with user identity
    gidOfUser = getgid();
    username = getUsername(uidOfUser);
    sshKeysDirInHost = getSshKeysDirInHost(username);
    dropbearDirInHost = libsarus::environment::getVariable("DROPBEAR_DIR");

    libsarus::filesystem::createFoldersIfNecessary(sshKeysDirInHost);
    libsarus::Lockfile lock{ sshKeysDirInHost }; // protect keys from concurrent writes

    if(userHasSshKeys() && !overwriteSshKeysIfExist) {
        auto message = boost::format("SSH keys not generated because they already exist in %s."
                                     " Use the '--overwrite' option to overwrite the existing keys.")
                        % sshKeysDirInHost;
        log(message, libsarus::LogLevel::GENERAL);
        return;
    }

    boost::filesystem::remove_all(sshKeysDirInHost);
    libsarus::filesystem::createFoldersIfNecessary(sshKeysDirInHost);
    sshKeygen(sshKeysDirInHost / "dropbear_ecdsa_host_key");
    sshKeygen(sshKeysDirInHost / "id_dropbear");
    generateAuthorizedKeys(sshKeysDirInHost / "id_dropbear", sshKeysDirInHost / "authorized_keys");

    log("Successfully generated SSH keys", libsarus::LogLevel::GENERAL);
    log("Successfully generated SSH keys", libsarus::LogLevel::INFO);
}

void SshHook::checkUserHasSshKeys() {
    log("Checking that user has SSH keys", libsarus::LogLevel::INFO);

    uidOfUser = getuid(); // "user-has-ssh-keys" command is executed with user identity
    gidOfUser = getgid();
    username = getUsername(uidOfUser);
    sshKeysDirInHost = getSshKeysDirInHost(username);

    if(!userHasSshKeys()) {
        log(boost::format{"Could not find SSH keys in %s"} % sshKeysDirInHost, libsarus::LogLevel::INFO);
        exit(EXIT_FAILURE); // exit with non-zero to communicate missing keys to calling process
    }

    log("Successfully checked that user has SSH keys", libsarus::LogLevel::INFO);
}

void SshHook::startStopSshDaemon() {
    log("Activating SSH in container", libsarus::LogLevel::INFO);

    dropbearRelativeDirInContainer = boost::filesystem::path("/opt/oci-hooks/ssh/dropbear");
    dropbearDirInHost = libsarus::environment::getVariable("DROPBEAR_DIR");
    serverPort = getServerPortFromEnv();
    containerState = libsarus::hook::parseStateOfContainerFromStdin();
    parseConfigJSONOfBundle();

    auto joinNamespaces = eventuallyJoinNamespaces(containerState.pid());

    if(containerState.status() == "stopped" and !joinNamespaces) {
        return stopSshDaemon();
    }
    
    if (joinNamespaces) {
        libsarus::hook::enterMountNamespaceOfProcess(containerState.pid());
        libsarus::hook::enterPidNamespaceOfProcess(containerState.pid());
    }

    username = getUsername(uidOfUser);
    sshKeysDirInHost = getSshKeysDirInHost(username);
    sshKeysDirInContainer = getSshKeysDirInContainer();
    copyDropbearIntoContainer();
    setupSshKeysDirInContainer();
    copySshKeysIntoContainer();
    patchPasswdIfNecessary();
    createEnvironmentFile();
    createEtcProfileModule();
    startSshDaemonInContainer();
    createSshExecutableInContainer();

    log("Successfully activated SSH in container", libsarus::LogLevel::INFO);
}

void SshHook::parseConfigJSONOfBundle() {
    log("Parsing bundle's config.json", libsarus::LogLevel::INFO);

    auto json = libsarus::json::read(containerState.bundle() / "config.json");

    libsarus::hook::applyLoggingConfigIfAvailable(json);

    // get rootfs
    auto root = boost::filesystem::path{ json["root"]["path"].GetString() };
    if(root.is_absolute()) {
        rootfsDir = root;
    }
    else {
        rootfsDir = containerState.bundle() / root;
    }

    dropbearDirInContainer = rootfsDir / dropbearRelativeDirInContainer;

    // get uid + gid of user
    uidOfUser = json["process"]["user"]["uid"].GetInt();
    gidOfUser = json["process"]["user"]["gid"].GetInt();

    if(json.HasMember("annotations")) {
        if(json["annotations"].HasMember("com.hooks.ssh.authorize_ssh_key")) {
            userPublicKeyFilename = boost::filesystem::path(json["annotations"]["com.hooks.ssh.authorize_ssh_key"].GetString());
        }
        if(json["annotations"].HasMember("com.hooks.ssh.pidfile_container")) {
            pidfileContainer = boost::filesystem::path(json["annotations"]["com.hooks.ssh.pidfile_container"].GetString());
        }
        if(json["annotations"].HasMember("com.hooks.ssh.pidfile_host")) {
            pidfileHost = boost::filesystem::path(json["annotations"]["com.hooks.ssh.pidfile_host"].GetString());
        }
        if(json["annotations"].HasMember("com.hooks.ssh.port")) {
            serverPort = std::stoi(json["annotations"]["com.hooks.ssh.port"].GetString());
        }
    }

    if (pidfileHost.empty()) {
        pidfileHost = containerState.bundle() / 
        boost::filesystem::path((boost::format("dropbear-%s-%s-%d.pid") % libsarus::process::getHostname() % containerState.id() % serverPort).str());
    }

    log("Successfully parsed bundle's config.json", libsarus::LogLevel::INFO);
}

bool SshHook::userHasSshKeys() const {
    auto expectedKeyFiles = std::vector<std::string>{"dropbear_ecdsa_host_key", "id_dropbear", "authorized_keys"};
    for(const auto& file : expectedKeyFiles) {
        auto fullPath = sshKeysDirInHost / file;
        if(!boost::filesystem::exists(fullPath)) {
            auto message = boost::format{"Expected SSH key file %s not found"} % fullPath;
            log(message, libsarus::LogLevel::DEBUG);
            return false;
        }
    }
    log(boost::format{"Found SSH keys in %s"} % sshKeysDirInHost, libsarus::LogLevel::DEBUG);
    return true;
}

std::string SshHook::getUsername(uid_t uid) const {
    auto passwdFile = boost::filesystem::path{ libsarus::environment::getVariable("PASSWD_FILE") };
    return libsarus::PasswdDB{passwdFile}.getUsername(uidOfUser);
}

boost::filesystem::path SshHook::getSshKeysDirInHost(const std::string& username) const {
    auto baseDir = boost::filesystem::path{ libsarus::environment::getVariable("HOOK_BASE_DIR") };
    return baseDir / username / ".oci-hooks/ssh/keys";
}

boost::filesystem::path SshHook::getSshKeysDirInContainer() const {
    // For an explanation of the logic within this function please consult
    // https://sarus.readthedocs.io/en/latest/developer/ssh.html#determining-the-location-of-the-ssh-keys-inside-the-container
    auto homeDirectory = libsarus::PasswdDB{rootfsDir / "etc/passwd"}.getHomeDirectory(uidOfUser);

    if (homeDirectory.empty() || homeDirectory == boost::filesystem::path{"/nonexistent"}) {
        log(boost::format("SSH Hook: Found invalid home directory in container's /etc/passwd for user %s (%d): \"%s\"")
            % username % uidOfUser % homeDirectory,
            libsarus::LogLevel::GENERAL);
        exit(EXIT_FAILURE);
    }

    auto sshKeysFullPath = rootfsDir / homeDirectory / ".ssh";
    log(boost::format("Setting SSH keys directory in container to %s") % sshKeysFullPath,
        libsarus::LogLevel::DEBUG);
    return sshKeysFullPath;
}

void SshHook::sshKeygen(const boost::filesystem::path& outputFile) const {
    log(boost::format{"Generating %s"} % outputFile, libsarus::LogLevel::INFO);
    auto command = boost::format{"%s/bin/dropbearkey -t ecdsa -f %s"}
        % dropbearDirInHost.string()
        % outputFile.string();
    libsarus::process::executeCommand(command.str());
}

static void authorizePublicKey(const boost::filesystem::path& publicKeyFileName,
                        const boost::filesystem::path& authorizedKeysFileName) {
    std::ifstream publicKeyFile(publicKeyFileName.string());
    std::ofstream authorizedKeysFile(authorizedKeysFileName.string(), std::ios::app);
    authorizedKeysFile << publicKeyFile.rdbuf();
}

void SshHook::generateAuthorizedKeys(const boost::filesystem::path& userKeyFile,
                                     const boost::filesystem::path& authorizedKeysFile) const {
    log(boost::format{"Generating \"authorized_keys\" file (%s)"} % authorizedKeysFile,
        libsarus::LogLevel::INFO);

    // output user's public key
    auto command = boost::format{"%s/bin/dropbearkey -y -f %s"}
        % dropbearDirInHost.string()
        % userKeyFile.string();
    auto output = libsarus::process::executeCommand(command.str());

    // extract public key
    auto ss = std::stringstream{ output };
    auto line = std::string{};
    auto matches = boost::smatch{};
    auto re = boost::regex{"^(ecdsa-.*)$"};

    libsarus::filesystem::createFileIfNecessary(authorizedKeysFile, uidOfUser, gidOfUser);

    // write public key to "authorized_keys" file
    while(std::getline(ss, line)) {
        if(boost::regex_match(line, matches, re)) {
            auto ofs = std::ofstream{authorizedKeysFile.c_str(), std::ios::out | std::ios::app};
            ofs << matches[1] << std::endl;
            ofs.close();
            log("Successfully generated \"authorized_keys\" file", libsarus::LogLevel::INFO);
            return;
        }
    }

    // set permissions
    boost::filesystem::permissions(authorizedKeysFile,
        boost::filesystem::owner_read | boost::filesystem::owner_write |
        boost::filesystem::group_read |
        boost::filesystem::others_read);

    auto message = boost::format("Failed to parse key from %s") % userKeyFile;
    SARUS_THROW_ERROR(message.str());
}

void SshHook::copyDropbearIntoContainer() const {
    log(boost::format("Copying Dropbear binaries into container under %s") % dropbearDirInContainer,
        libsarus::LogLevel::INFO);

    libsarus::filesystem::copyFile(dropbearDirInHost / "bin/dbclient", dropbearDirInContainer / "bin/dbclient");
    libsarus::filesystem::copyFile(dropbearDirInHost / "bin/dropbear", dropbearDirInContainer / "bin/dropbear");

    log("Successfully copied Dropbear binaries into container", libsarus::LogLevel::INFO);
}

void SshHook::setupSshKeysDirInContainer() const {
    log(boost::format("Setting up directory for SSH keys into container under %s") % sshKeysDirInContainer,
        libsarus::LogLevel::INFO);

    auto rootIdentity = libsarus::UserIdentity{};
    auto userIdentity = libsarus::UserIdentity(uidOfUser, gidOfUser, {});

    // switch to unprivileged user to make sure that the user has the
    // permission to create a new folder ~/.ssh in the container
    libsarus::process::switchIdentity(userIdentity);
    libsarus::filesystem::createFoldersIfNecessary(sshKeysDirInContainer);
    libsarus::process::switchIdentity(rootIdentity);

    bool overlayMountHostDotSsh = true;
    try {
        auto envOverlayMountHome = libsarus::environment::getVariable("OVERLAY_MOUNT_HOME_SSH");
        overlayMountHostDotSsh = boost::algorithm::to_upper_copy(envOverlayMountHome) != "FALSE";
    } 
    catch(const libsarus::Error& error) {
        auto message = boost::format("%s. ~/.ssh will be mounted in the container using OverlayFS.") % error.what();
        log(message, libsarus::LogLevel::INFO);
    }

    if (overlayMountHostDotSsh) {
        // mount overlayfs on top of the container's ~/.ssh, otherwise we
        // could mess up with the host's ~/.ssh directory. E.g. when the user
        // bind mounts the host's /home into the container
        auto lowerDir = containerState.bundle() / "overlay/ssh-lower";
        auto upperDir = containerState.bundle() / "overlay/ssh-upper";
        auto workDir = containerState.bundle() / "overlay/ssh-work";
        libsarus::filesystem::createFoldersIfNecessary(lowerDir);
        libsarus::filesystem::createFoldersIfNecessary(upperDir, uidOfUser, gidOfUser);
        libsarus::filesystem::createFoldersIfNecessary(workDir);
        libsarus::mount::mountOverlayfs(lowerDir, upperDir, workDir, sshKeysDirInContainer);
    }
    
    log("Successfully set up directory for SSH keys into container", libsarus::LogLevel::INFO);
}

void SshHook::copySshKeysIntoContainer() const {
    log("Copying SSH keys into container", libsarus::LogLevel::INFO);

    // server keys
    libsarus::filesystem::copyFile(sshKeysDirInHost / "dropbear_ecdsa_host_key",
                            sshKeysDirInContainer / "dropbear_ecdsa_host_key",
                            uidOfUser, gidOfUser);

    // client keys
    libsarus::filesystem::copyFile(sshKeysDirInHost / "id_dropbear",
                            sshKeysDirInContainer / "id_dropbear",
                            uidOfUser, gidOfUser);

    // update if necessary
    auto containerAuthorizedKeys = sshKeysDirInContainer / "authorized_keys";
    libsarus::filesystem::copyFile(sshKeysDirInHost / "authorized_keys",
                            containerAuthorizedKeys,
                            uidOfUser, gidOfUser);
    if(!userPublicKeyFilename.empty()) {
        log(boost::format("Adding key %s to %s") % userPublicKeyFilename.string() % containerAuthorizedKeys.string(), libsarus::LogLevel::INFO);
        auto rootIdentity = libsarus::UserIdentity{};
        auto userIdentity = libsarus::UserIdentity(uidOfUser, gidOfUser, {});
        libsarus::process::switchIdentity(userIdentity);
        authorizePublicKey(boost::filesystem::path{userPublicKeyFilename}, containerAuthorizedKeys);
        libsarus::process::switchIdentity(rootIdentity);
    }

    log("Successfully copied SSH keys into container", libsarus::LogLevel::INFO);
}

void SshHook::createSshExecutableInContainer() const {
    log("Creating ssh binary (shell script) in container", libsarus::LogLevel::INFO);

    boost::filesystem::remove_all(rootfsDir / "usr/bin/ssh");

    // create file
    auto ofs = std::ofstream((rootfsDir / "usr/bin/ssh").c_str());
    ofs << "#!/bin/sh" << std::endl
        << dropbearRelativeDirInContainer.string() << "/bin/dbclient -y -p " << serverPort << " $*" << std::endl;
    ofs.close();

    // set permissions
    boost::filesystem::permissions(rootfsDir / "usr/bin/ssh",
        boost::filesystem::owner_all |
        boost::filesystem::group_read | boost::filesystem::group_exe |
        boost::filesystem::others_read | boost::filesystem::others_exe);

    log("Successfully created ssh binary in container", libsarus::LogLevel::INFO);
}

void SshHook::patchPasswdIfNecessary() const {
    log("Patching container's /etc/passwd if necessary"
        " (ensure that command interpreter is valid)", libsarus::LogLevel::INFO);

    auto passwd = libsarus::PasswdDB{rootfsDir / "etc/passwd"};
    for(auto& entry : passwd.getEntries()) {
        if( entry.userCommandInterpreter
            && !boost::filesystem::exists(rootfsDir / *entry.userCommandInterpreter)) {
            entry.userCommandInterpreter = "/bin/sh";
        }
    }
    passwd.write(rootfsDir / "etc/passwd");

    log("Successfully patched container's /etc/passwd", libsarus::LogLevel::INFO);
}

void SshHook::createEnvironmentFile() const {
    log(boost::format("Creating script to export container environment upon login in %s") %
        (dropbearDirInContainer / "environment"),
        libsarus::LogLevel::INFO);

    // create file
    auto containerEnvironment = libsarus::hook::parseEnvironmentVariablesFromOCIBundle(containerState.bundle());
    auto ofs = std::ofstream((dropbearDirInContainer / "environment").c_str());
    ofs << "#!/bin/sh" << std::endl;
    for (const auto& variable : containerEnvironment) {
        ofs << "export " << variable.first << "=\"" << variable.second << "\"" << std::endl;
    }
    ofs.close();

    // set permissions
    boost::filesystem::permissions(dropbearDirInContainer / "environment",
        boost::filesystem::owner_all |
        boost::filesystem::group_read |
        boost::filesystem::others_read );

    log("Successfully created script to export container environment upon login", libsarus::LogLevel::INFO);
}

void SshHook::createEtcProfileModule() const {
    log("Creating module in container's /etc/profile.d", libsarus::LogLevel::INFO);

    // create file
    auto ofs = std::ofstream((rootfsDir / "etc/profile.d/ssh-hook.sh").c_str());
    ofs << "#!/bin/sh" << std::endl
        << "if [ \"$SSH_CONNECTION\" ]; then" << std::endl
        << "    . " << dropbearRelativeDirInContainer.string() << "/environment" << std::endl
        << "fi" << std::endl;
    ofs.close();

    // set permissions
    boost::filesystem::permissions(rootfsDir / "etc/profile.d/ssh-hook.sh",
        boost::filesystem::owner_read | boost::filesystem::owner_write |
        boost::filesystem::group_read |
        boost::filesystem::others_read);

    log("Successfully created module in container's /etc/profile.d", libsarus::LogLevel::INFO);
}

void SshHook::startSshDaemonInContainer() const {
    log("Starting SSH daemon in container", libsarus::LogLevel::INFO);

    std::function<void()> preExecActions = [this]() {
        if(chroot(rootfsDir.c_str()) != 0) {
            auto message = boost::format("Failed to chroot to %s: %s")
                % rootfsDir % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }

        libsarus::hook::switchToUnprivilegedProcess(uidOfUser, gidOfUser);
    };

    auto sshKeysPathWithinContainer = "/" / boost::filesystem::relative(sshKeysDirInContainer, rootfsDir);

    auto pidfileContainerReal = libsarus::filesystem::realpathWithinRootfs(rootfsDir, pidfileContainer);
    auto pidfileContainerFull = rootfsDir / pidfileContainerReal;
    libsarus::filesystem::createFoldersIfNecessary(pidfileContainerFull.parent_path(), uidOfUser, gidOfUser);

    auto dropbearCommand = libsarus::CLIArguments{
        dropbearRelativeDirInContainer.string() + "/bin/dropbear",
        "-E",
        "-r", sshKeysPathWithinContainer.string() + "/dropbear_ecdsa_host_key",
        "-p", std::to_string(serverPort),
        "-P", pidfileContainerReal.string()
    };
    auto status = libsarus::process::forkExecWait(dropbearCommand, preExecActions);
    if(status != 0) {
        auto message = boost::format("%s/bin/dropbear exited with status %d")
            % dropbearRelativeDirInContainer % status;
        SARUS_THROW_ERROR(message.str());
    }


    if (!pidfileHost.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (boost::filesystem::is_regular_file(pidfileContainerFull)) {
            libsarus::filesystem::copyFile(pidfileContainerFull, pidfileHost, uidOfUser, gidOfUser);
            auto message = boost::format("Copied Dropbear pidfile to host path (%s)")
                    % pidfileHost;
            log(message, libsarus::LogLevel::INFO);
        }
        else {
            auto message = boost::format("Failed to copy Dropbear pidfile to host path (%s): container pidfile (%s) not found")
                    % pidfileHost % pidfileContainerReal;
            log(message, libsarus::LogLevel::WARN);
        }
    }

    log("Successfully started SSH daemon in container", libsarus::LogLevel::INFO);
}

void SshHook::stopSshDaemon() {

    auto pid = std::stoi(libsarus::filesystem::readFile(pidfileHost));

    log(boost::format("Deactivating SSH daemon with pidfile %s and PID %s")% pidfileHost % pid, libsarus::LogLevel::INFO);

    libsarus::filesystem::removeFile(pidfileHost);

    if(kill(getpgid(pid), SIGTERM) == 0 ) {
        return;
    }
    if(kill(pid, SIGTERM) == 0) {
        return;
    }

    auto message = boost::format("Unable to kill Dropbear process with PID %d") % pid;
    SARUS_THROW_ERROR(message.str());
}

void SshHook::log(const boost::format& message, libsarus::LogLevel level) const {
    log(message.str(), level);
}

void SshHook::log(const std::string& message, libsarus::LogLevel level) const {
    auto subsystemName = "SSH hook";
    libsarus::Logger::getInstance().log(message, subsystemName, level);
}

}}} // namespace
