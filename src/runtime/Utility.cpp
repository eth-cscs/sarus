/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Utility.hpp"

#include <signal.h>

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
                logMessage(message, common::LogLevel::DEBUG);

                // Restore the default signal handler and forward the signal to this process so it's not lost
                signal(signo, SIG_DFL);
                kill(getpid(), signo);
            }
            else {
                auto message = boost::format("Error forwarding signal %d to OCI runtime (PID %d): %s") % signo % target % strerror(errno);
                logMessage(message, common::LogLevel::ERROR);
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
            logMessage(message, common::LogLevel::WARN);
        }
    }
};

void logMessage(const boost::format& message, common::LogLevel level,
                std::ostream& out, std::ostream& err) {
    logMessage(message.str(), level, out, err);
}

void logMessage(const std::string& message, common::LogLevel level,
                std::ostream& out, std::ostream& err) {
    auto subsystemName = "runtime";
    common::Logger::getInstance().log(message, subsystemName, level, out, err);
}

} // namespace
} // namespace
} // namespace
