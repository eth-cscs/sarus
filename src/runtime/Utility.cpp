/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Utility.hpp"

#include <signal.h>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

#include "libsarus/Error.hpp"
#include "libsarus/Utility.hpp"


namespace sarus {
namespace runtime {
namespace utility {

class SignalProxy{
public:
    static pid_t target;
    static void proxySignal(int signo) {
        if (kill(target, signo) == -1) {
            if (errno == ESRCH) {
                auto message = boost::format("Could not forward signal %d to OCI runtime (PID %d): process does not exist") % signo % target;
                utility::logMessage(message, libsarus::LogLevel::DEBUG);

                // Restore the default signal handler and forward the signal to this process so it's not lost
                signal(signo, SIG_DFL);
                kill(getpid(), signo);
            }
            else {
                auto message = boost::format("Error forwarding signal %d to OCI runtime (PID %d): %s") % signo % target % strerror(errno);
                utility::logMessage(message, libsarus::LogLevel::ERROR);
            }
        }
     };
};

pid_t SignalProxy::target = getpid();

void setupSignalProxying(const pid_t childPid) {
    SignalProxy::target = childPid;

    // Proxy all signals, except SIGCHLD and SIGPIPE,
    // which are most likely intended for the engine itself.
    // Preprocessor conditions handle signals which are not defined in all architectures,
    // see signal(7) man page for reference
    auto signalList = std::vector<int>{
        SIGABRT,
        SIGALRM,
        SIGBUS,
        SIGCONT,
        #ifdef SIGEMT
        SIGEMT,
        #endif
        SIGFPE,
        SIGHUP,
        SIGILL,
        #ifdef SIGINFO
        SIGINFO,
        #endif
        SIGINT,
        SIGIO,
        SIGIOT,
        #ifdef SIGLOST
        SIGLOST,
        #endif
        SIGPOLL,
        SIGPROF,
        SIGPWR,
        SIGQUIT,
        SIGSEGV,
        #ifdef SIGSTKFLT
        SIGSTKFLT,
        #endif
        SIGTSTP,
        SIGSYS,
        SIGTERM,
        SIGTRAP,
        SIGTTIN,
        SIGTTOU,
        #ifdef SIGUNUSED
        SIGUNUSED,
        #endif
        SIGURG,
        SIGUSR1,
        SIGUSR2,
        SIGVTALRM,
        SIGXCPU,
        SIGXFSZ,
        SIGWINCH
    };

    // Use the sigaction struct and syscall for better portability;
    // use BSD semantics to implement default glibc 2+ behavior.
    // For references, see signal(2) (Portability section) and sigaction(2) man pages,
    // and https://www.gnu.org/software/libc/manual/html_node/Sigaction-Function-Example.html
    struct sigaction proxyAction;
    proxyAction.sa_handler = SignalProxy::proxySignal;
    sigemptyset(&proxyAction.sa_mask);
    proxyAction.sa_flags = SA_RESTART;

    for (const auto signalNumber : signalList) {
        if (sigaction(signalNumber, &proxyAction, nullptr) == -1) {
            auto message = boost::format("Error setting up forwarding for signal %d to OCI runtime (PID %d): %s")
                % signalNumber % childPid % strerror(errno);
            utility::logMessage(message, libsarus::LogLevel::WARN);
        }
    }
};

std::vector<std::unique_ptr<libsarus::Mount>> generatePMIxMounts(std::shared_ptr<const common::Config> config) {
    auto mounts = std::vector<std::unique_ptr<libsarus::Mount>>{};
    auto& hostEnvironment = config->commandRun.hostEnvironment;

    auto pmixServerPath = boost::filesystem::path{};
    auto pmixServerVar = hostEnvironment.find("PMIX_SERVER_TMPDIR");
    if (pmixServerVar != hostEnvironment.cend()) {
        utility::logMessage(boost::format("Found PMIX_SERVER_TMPDIR=%s") % pmixServerVar->second, libsarus::LogLevel::DEBUG);
        pmixServerPath = boost::filesystem::path(pmixServerVar->second);
        mounts.push_back(std::unique_ptr<libsarus::Mount>{new libsarus::Mount{pmixServerPath, pmixServerPath, MS_REC|MS_PRIVATE, config->getRootfsDirectory(), config->userIdentity}});
    }
    else {
        utility::logMessage("Could not find PMIX_SERVER_TMPDIR env variable", libsarus::LogLevel::DEBUG);
    }

    auto slurmMpiType = hostEnvironment.find("SLURM_MPI_TYPE");
    if (slurmMpiType != hostEnvironment.cend()) {
        boost::smatch matches;
        if (boost::regex_match(slurmMpiType->second, matches, boost::regex{"^pmix*"})) {
            try {
                auto slurmConfig = libsarus::process::executeCommand("scontrol show config");
                auto slurmJobId  = hostEnvironment.at("SLURM_JOB_ID");
                auto slurmJobUid = hostEnvironment.at("SLURM_JOB_UID");
                auto slurmStepId = hostEnvironment.at("SLURM_STEP_ID");

                if (boost::regex_match(slurmConfig, matches, boost::regex{"SlurmdSpoolDir\\s*=\\s(.*)"})) {
                    utility::logMessage(boost::format("Found SlurmdSpoolDir=%s") % matches[1], libsarus::LogLevel::DEBUG);
                    auto slurmSpoolPath = boost::filesystem::path(matches[1]);
                    auto slurmPmixPath = slurmSpoolPath / (boost::format("pmix.%s.%s") % slurmJobId % slurmStepId).str();
                    // Check that the path under Slurm's spool dir is not equal or child of the PMIx server tempdir
                    // we have already scheduled for mounting
                    auto relativePath = boost::filesystem::relative(slurmPmixPath, pmixServerPath);
                    if (relativePath.empty() || boost::starts_with(relativePath.string(), std::string(".."))) {
                        mounts.push_back(std::unique_ptr<libsarus::Mount>{new libsarus::Mount{slurmPmixPath, slurmPmixPath, MS_REC|MS_PRIVATE, config->getRootfsDirectory(), config->userIdentity}});
                    }
                    else {
                        utility::logMessage("Slurm PMIx directory for job step is equal or child of PMIX_SERVER_TMPDIR. Skipping mount",
                                            libsarus::LogLevel::DEBUG);
                    }
                }

                if (boost::regex_match(slurmConfig, matches, boost::regex{"TmpFS\\s*=\\s(.*)"})) {
                    utility::logMessage(boost::format("Found Slurm TmpFS=%s") % matches[1], libsarus::LogLevel::DEBUG);
                    auto slurmTmpFS = boost::filesystem::path(matches[1]);
                    auto mountPath = slurmTmpFS / (boost::format("spmix_appdir_%s_%s.%s") % slurmJobUid % slurmJobId % slurmStepId).str();
                    if (boost::filesystem::exists(mountPath)) {
                        mounts.push_back(std::unique_ptr<libsarus::Mount>{new libsarus::Mount{mountPath, mountPath, MS_REC|MS_PRIVATE, config->getRootfsDirectory(), config->userIdentity}});
                    }
                    else{
                        mountPath = slurmTmpFS / (boost::format("spmix_appdir_%s.%s") % slurmJobId % slurmStepId).str();
                        mounts.push_back(std::unique_ptr<libsarus::Mount>{new libsarus::Mount{mountPath, mountPath, MS_REC|MS_PRIVATE, config->getRootfsDirectory(), config->userIdentity}});
                    }
                }
            }
            catch (libsarus::Error& e) {
                auto message = boost::format("Error generating Slurm-specific PMIx v3 mounts: %s.\nAttempting to continue...") % e.what();
                utility::logMessage(message, libsarus::LogLevel::WARN);
            }
        }
    }

    return mounts;
}

void logMessage(const boost::format& message, libsarus::LogLevel level,
                std::ostream& out, std::ostream& err) {
    utility::logMessage(message.str(), level, out, err);
}

void logMessage(const std::string& message, libsarus::LogLevel level,
                std::ostream& out, std::ostream& err) {
    auto subsystemName = "Runtime";
    libsarus::Logger::getInstance().log(message, subsystemName, level, out, err);
}

} // namespace
} // namespace
} // namespace
