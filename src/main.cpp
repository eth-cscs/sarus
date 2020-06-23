/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
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
#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "common/Utility.hpp"
#include "cli/CLI.hpp"
#include "runtime/SecurityChecks.hpp"

#include <sys/types.h>
#include <sys/stat.h>

using namespace sarus;

static void saveAndClearEnvironmentVariables(common::Config& config);
static void dropPrivileges(const common::Config& config);
static void getPrivileges();

int main(int argc, char* argv[]) {
    std::setlocale(LC_CTYPE, "C.UTF-8"); // enable handling of non-ascii characters

    // TODO: we should also consider to explicitely set the permissions in sarus::common::createFileIfNecessary,
    //       sarus::common::createFolderIfNecesasary, sarus::common::copyFile, sarus::common::copyFolder, etc.
    //       Note: sarus::common::copyFile and sarus::common::copyFolder should probably preserve
    //       the same permissions as the source.
    umask(022);

    auto& logger = common::Logger::getInstance();

    try {
        auto program_start = std::chrono::high_resolution_clock::now();

        // Initialize Config object
        auto sarusInstallationPrefixDir = boost::filesystem::canonical("/proc/self/exe").parent_path().parent_path();
        auto config = std::make_shared<sarus::common::Config>(sarusInstallationPrefixDir);
        config->program_start = program_start;
        runtime::SecurityChecks{config}.runSecurityChecks(sarusInstallationPrefixDir);
        saveAndClearEnvironmentVariables(*config);


        // Process command
        auto args = common::CLIArguments(argc, argv);
        auto command = cli::CLI{}.parseCommandLine(args, config);
        if(!command->requiresRootPrivileges()) {
            dropPrivileges(*config);
        }
        else {
            getPrivileges();
        }
        command->execute();
    }
    catch(const common::Error& e) {
        logger.logErrorTrace(e, "main");
        return 1;
    }
    catch(const std::exception& e) {
        auto message = boost::format("Caught exception in main function. No error trace available."
                                     " Exception message: %s") % e.what();
        logger.log(message.str(), "main", common::LogLevel::ERROR);
        return 1;
    }

    return 0;
}

static void saveAndClearEnvironmentVariables(common::Config& config) {
    config.commandRun.hostEnvironment = common::parseEnvironmentVariables(environ);

    if(clearenv() != 0) {
        SARUS_THROW_ERROR("Failed to clear host environment variables");
    }

    auto path = std::string{"/bin:/sbin:/usr/bin"};
    int overwrite = 1;
    if(setenv("PATH", path.c_str(), overwrite) != 0) {
        auto message = boost::format("Failed to setenv(\"PATH\", %s, %d): %s")
            % path.c_str() % overwrite % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
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
