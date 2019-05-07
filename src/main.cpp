/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <exception>
#include <iostream>
#include <memory>
#include <chrono>

#include <unistd.h>
#include <sys/prctl.h>

#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "common/Utility.hpp"
#include "cli/CLI.hpp"


using namespace sarus;

static void saveAndClearEnvironmentVariables(common::Config& config);
static void dropPrivileges(const common::Config& config);
static void getPrivileges();

int main(int argc, char* argv[]) {
    auto& logger = common::Logger::getInstance();

    try {
        auto conf = std::make_shared<common::Config>();
        saveAndClearEnvironmentVariables(*conf);
        conf->program_start = std::chrono::high_resolution_clock::now();
        conf->json.initialize(conf->buildTime.configFile, conf->buildTime.configSchemaFile);

        auto args = common::CLIArguments(argc, argv);
        auto command = cli::CLI{}.parseCommandLine(args, conf);

        if(!command->requiresRootPrivileges()) {
            dropPrivileges(*conf);
        }
        else {
            getPrivileges();
        }

        command->execute();
    }
    catch(const common::Error& e) {
        logger.log("Caught exception in main function", "main", common::logType::ERROR);
        logger.logErrorTrace(e, "main");
        return 1;
    }
    catch(const std::exception& e) {
        auto message = boost::format("Caught exception in main function. No error trace available."
                                     " Exception message: %s") % e.what();
        logger.log(message.str(), "main", common::logType::ERROR);
        return 1;
    }

    return 0;
}

static void saveAndClearEnvironmentVariables(common::Config& config) {
    config.commandRun.hostEnvironment = common::parseEnvironmentVariables(environ);
    if(clearenv() != 0) {
        SARUS_THROW_ERROR("Failed to clear host environment variables");
    }
    auto path = "PATH=/bin:/sbin:/usr/bin";
    if(putenv(strdup(path)) != 0) {
        auto message = boost::format("Failed to putenv(%s): %s") % path % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}

static void dropPrivileges(const common::Config& config) {
    auto uid = config.userIdentity.uid;
    if(setresuid(uid, uid, uid) != 0) {
        auto message = boost::format("Failed to setresuid(%1%, %1%, %1%): %s") % uid % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    auto gid = config.userIdentity.gid;
    if(setresgid(gid, gid, gid) != 0) {
        auto message = boost::format("Failed to setresgid(%1%, %1%, %1%): %s") % gid % strerror(errno);
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
