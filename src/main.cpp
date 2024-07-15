/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <exception>
#include <iostream>
#include <memory>
#include <chrono>
#include <clocale>

#include <unistd.h>
#include <sys/prctl.h>

#include "common/Config.hpp"
#include "libsarus/Error.hpp"
#include "libsarus/Logger.hpp"
#include "libsarus/Utility.hpp"
#include "cli/CLI.hpp"
#include "runtime/SecurityChecks.hpp"

#include <sys/types.h>
#include <sys/stat.h>

using namespace sarus;

static void dropPrivileges(const common::Config& config);
static void getPrivileges();

int main(int argc, char* argv[]) {
    std::setlocale(LC_CTYPE, "C.UTF-8"); // enable handling of non-ascii characters

    // TODO: we should also consider to explicitely set the permissions in sarus::common::createFileIfNecessary,
    //       sarus::common::createFolderIfNecesasary, sarus::common::copyFile, sarus::common::copyFolder, etc.
    //       Note: sarus::common::copyFile and sarus::common::copyFolder should probably preserve
    //       the same permissions as the source.
    umask(022);

    auto& logger = libsarus::Logger::getInstance();

    try {
        auto program_start = std::chrono::high_resolution_clock::now();

        // Initialize Config object
        auto sarusInstallationPrefixDir = boost::filesystem::canonical("/proc/self/exe").parent_path().parent_path();
        auto config = std::make_shared<sarus::common::Config>(sarusInstallationPrefixDir);
        config->program_start = program_start;
        runtime::SecurityChecks{config}.runSecurityChecks(sarusInstallationPrefixDir);
        config->commandRun.hostEnvironment = libsarus::environment::parseVariables(environ);


        // Process command
        auto args = libsarus::CLIArguments(argc, argv);
        auto command = cli::CLI{}.parseCommandLine(args, config);
        if(!command->requiresRootPrivileges()) {
            dropPrivileges(*config);
        }
        else {
            getPrivileges();
        }
        command->execute();
    }
    catch(const libsarus::Error& e) {
        logger.logErrorTrace(e, "main");
        return 1;
    }
    catch(const std::exception& e) {
        auto message = boost::format("Caught exception in main function. No error trace available."
                                     " Exception message: %s") % e.what();
        logger.log(message.str(), "main", libsarus::LogLevel::ERROR);
        return 1;
    }

    return 0;
}

static void dropPrivileges(const common::Config& config) {
    auto uid = config.userIdentity.uid;
    if(setresuid(uid, uid, uid) != 0) {
        auto message = boost::format("Failed to setresuid(%1%, %1%, %1%): %2%") % uid % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    auto gid = config.userIdentity.gid;
    if(setresgid(gid, gid, gid) != 0) {
        auto message = boost::format("Failed to setresgid(%1%, %1%, %1%): %2%") % gid % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        auto message = boost::format("Failed to set no_new_privs bit: %s") % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}

static void getPrivileges() {
    // Set real uid/gid to 0 (effective uid/gid are already 0 because this program is SUID root).
    // The real uid/gid have to be 0 as well, otherwise some mount operations will fail.

    if(setreuid(0, 0) != 0) {
        auto message = boost::format("Failed to setreuid(0, 0): %s") % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    if(setregid(0, 0) != 0) {
        auto message = boost::format("Failed to setregid(0, 0): %s") % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}
