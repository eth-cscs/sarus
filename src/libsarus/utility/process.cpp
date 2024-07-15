/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "process.hpp"

#include <sstream>

#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <termios.h>
#include <sys/fsuid.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "libsarus/Error.hpp"
#include "libsarus/PasswdDB.hpp"
#include "libsarus/utility/logging.hpp"
#include "libsarus/utility/filesystem.hpp"

/**
 * Utility functions for system operations 
 */

namespace libsarus {
namespace process {

static void logProcessUserAndGroupIdentifiers() {
    uid_t ruid, euid, suid;
    if (getresuid(&ruid, &euid, &suid) != 0){
      SARUS_THROW_ERROR("getresuid failed");
    }
    gid_t rgid, egid, sgid;
    if (getresgid(&rgid, &egid, &sgid) != 0){
      SARUS_THROW_ERROR("getresgid failed");
    }
    logMessage(boost::format("Current uids (r/e/s/fs): %d %d %d %d")
        % ruid % euid % suid % setfsuid(-1), libsarus::LogLevel::DEBUG);
    logMessage(boost::format("Current gids (r/e/s/fs): %d %d %d %d")
        % rgid % egid % sgid % setfsgid(-1), libsarus::LogLevel::DEBUG);
}


void switchIdentity(const libsarus::UserIdentity& identity) {
    logProcessUserAndGroupIdentifiers();

    logMessage(boost::format{"Switching to identity (uid=%d gid=%d)"}
               % identity.uid % identity.gid,
               LogLevel::DEBUG);

    uid_t euid = geteuid();
    uid_t egid = getegid();

    if (euid == 0){
        // unprivileged processes cannot call setgroups
        if (setgroups(identity.supplementaryGids.size(), identity.supplementaryGids.data()) != 0) {
            SARUS_THROW_ERROR("Failed to setgroups");
        }
    }

    if (setegid(identity.gid) != 0) {
        SARUS_THROW_ERROR("Failed to setegid");
    }

    if (seteuid(identity.uid) != 0) {
        if (setegid(egid) != 0) {
            SARUS_THROW_ERROR("Failed to seteuid and Failed to restore egid");
        }
        SARUS_THROW_ERROR("Failed to seteuid");
    }

    logProcessUserAndGroupIdentifiers();
    logMessage("Successfully switched identity", LogLevel::DEBUG);
}

/*
 * Set the filesystem user ID to the uid in the provided UserIdentity
 *
 * Normally the filesystem user ID (or fsuid for short) coincides with the
 * effective user ID (euid) and is changed by the kernel when the euid is set,
 * as described in the Linux man pages:
 * https://man7.org/linux/man-pages/man2/setfsuid.2.html
 * https://man7.org/linux/man-pages/man7/credentials.7.html
 *
 * However, when having to bind-mount files which reside on root_squashed filesystems,
 * a process needs to have both root privileges (to perform the mount) and normal
 * user filesystem permissions (under root_squash, root is remapped to nobody and
 * cannot access the user content unless said content is world-readable).
 * The above is the main scenario in which this function is meant to be used.
 * Other similar use cases where both root privileges and user permissions are
 * required might occur.
 */
void setFilesystemUid(const libsarus::UserIdentity& identity) {
    logMessage(boost::format{"Setting filesystem uid to %d"} % identity.uid,
               LogLevel::DEBUG);

    setfsuid(identity.uid);
    if (setfsuid(identity.uid) != int(identity.uid)){
        SARUS_THROW_ERROR("Failed to set filesystem uid");
    }

    logMessage("Successfully set filesystem uid", LogLevel::DEBUG);
}

static void readCStream(FILE* const in, std::iostream* const out) {
    char buffer[1024];
    while(!feof(in)) {
        if(fgets(buffer, sizeof(buffer), in)) {
            *out << buffer;
        }
        else if(!feof(in)) {
            SARUS_THROW_ERROR("Failed to read C stream: call to fgets() failed.");
        }
    }
}

std::string executeCommand(const std::string& command) {
    auto commandWithRedirection = command + " 2>&1"; // stderr-to-stdout redirection necessary because popen only reads stdout
    logMessage(boost::format("Executing command '%s'") % commandWithRedirection, libsarus::LogLevel::DEBUG);

    FILE* pipe = popen(commandWithRedirection.c_str(), "r");
    if(!pipe) {
        auto message = boost::format("Failed to execute command \"%s\". Call to popen() failed (%s)")
            % commandWithRedirection % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    std::stringstream commandOutput;
    try {
        readCStream(pipe, &commandOutput);
    } catch(const libsarus::Error& e) {
        auto message = boost::format("Failed to read stdout from command \"%s\"") % commandWithRedirection;
        SARUS_RETHROW_ERROR(e, message.str());
    }

    auto status = pclose(pipe);
    if(status == -1) {
        auto message = boost::format("Failed to execute command \"%s\". Call to pclose() failed (%s)")
            % commandWithRedirection % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    else if(!WIFEXITED(status)) {
        auto message = boost::format(   "Failed to execute command \"%s\"."
                                        " Process terminated abnormally. Process' output:\n\n%s")
                                        % commandWithRedirection % commandOutput.str();
        SARUS_THROW_ERROR(message.str());
    }
    else if(WEXITSTATUS(status) != 0) {
        auto message = boost::format(   "Failed to execute command \"%s\"."
                                        " Process terminated with status %d. Process' output:\n\n%s")
                                        % commandWithRedirection % WEXITSTATUS(status) % commandOutput.str();
        SARUS_THROW_ERROR(message.str());
    }

    return commandOutput.str();
}

int forkExecWait(const libsarus::CLIArguments& args,
                 const boost::optional<std::function<void()>>& preExecChildActions,
                 const boost::optional<std::function<void(int)>>& postForkParentActions,
                 std::iostream* const childStdoutStream) {
    logMessage(boost::format("Forking and executing '%s'") % args, libsarus::LogLevel::DEBUG);


    int pipefd[2];
    if(childStdoutStream) {
        if(pipe(pipefd) == -1) {
            auto message = boost::format("Failed to open pipe to execute subprocess %s: %s")
                % args % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }
    }

    // fork and execute
    auto pid = fork();
    if(pid == -1) {
        auto message = boost::format("Failed to fork to execute subprocess %s: %s")
            % args % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    bool isChild = pid == 0;
    if(isChild) {
        if(childStdoutStream) {
            // Redirect stdout to write to the pipe, then we don't need the pipe ends anymore
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[0]);
            close(pipefd[1]);
        }
        if(preExecChildActions) {
            (*preExecChildActions)();
        }
        execvp(args.argv()[0], args.argv());
        auto message = boost::format("Failed to execvp subprocess %s: %s") % args % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    else {
        if(postForkParentActions) {
            (*postForkParentActions)(pid);
        }
        if(childStdoutStream) {
            // Close the write end of the pipe, as it won't be used
            close(pipefd[1]);

            FILE *childStdoutPipe = fdopen(pipefd[0], "r");
            try {
                readCStream(childStdoutPipe, childStdoutStream);
            } catch(const libsarus::Error& e) {
                auto message = boost::format("Failed to read stdout from subprocess %s") % args;
                SARUS_RETHROW_ERROR(e, message.str());
            }
        }
        int status;
        do {
            if(waitpid(pid, &status, 0) == -1) {
                auto message = boost::format("Failed to waitpid subprocess %s: %s")
                    % args % strerror(errno);
                SARUS_THROW_ERROR(message.str());
            }
        } while(!WIFEXITED(status) && !WIFSIGNALED(status));

        if(!WIFEXITED(status)) {
            auto message = boost::format("Subprocess %s terminated abnormally")
                % args;
            SARUS_THROW_ERROR(message.str());
        }

        logMessage( boost::format("%s (pid %d) exited with status %d") % args % pid % WEXITSTATUS(status),
                    libsarus::LogLevel::DEBUG);

        return WEXITSTATUS(status);
    }
}

std::string getHostname() {
    char hostname[HOST_NAME_MAX];
    if(gethostname(hostname, HOST_NAME_MAX) != 0) {
        auto message = boost::format("failed to retrieve hostname (%s)") % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    hostname[HOST_NAME_MAX-1] = '\0';
    return hostname;
}

std::vector<int> getCpuAffinity() {
    logMessage("Getting CPU affinity (list of CPU ids)", libsarus::LogLevel::INFO);

    auto set = cpu_set_t{};

    if(sched_getaffinity(getpid(), sizeof(cpu_set_t), &set) != 0) {
        auto message = boost::format("sched_getaffinity failed: %s ") % std::strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    auto cpus = std::vector<int>{};

    for(int cpu=0; cpu<CPU_SETSIZE; ++cpu) {
        if(CPU_ISSET(cpu, &set)) {
            cpus.push_back(cpu);
            logMessage(boost::format("Detected CPU %d") % cpu, libsarus::LogLevel::DEBUG);
        }
    }

    logMessage("Successfully got CPU affinity", libsarus::LogLevel::INFO);

    return cpus;
}

void setCpuAffinity(const std::vector<int>& cpus) {
    logMessage("Setting CPU affinity", libsarus::LogLevel::INFO);

    auto set = cpu_set_t{};
    CPU_ZERO(&set);

    for(auto cpu : cpus) {
        CPU_SET(cpu, &set);
        logMessage(boost::format("Set CPU %d") % cpu, libsarus::LogLevel::DEBUG);
    }

    if(sched_setaffinity(getpid(), sizeof(cpu_set_t), &set) != 0) {
        auto message = boost::format{"sched_setaffinity failed: %s"} % std::strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    logMessage("Successfully set CPU affinity", libsarus::LogLevel::INFO);
}

void setStdinEcho(bool flag)
{
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    if( !flag ) {
        tty.c_lflag &= ~ECHO;
    }
    else {
        tty.c_lflag |= ECHO;
    }

    (void) tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

}}
