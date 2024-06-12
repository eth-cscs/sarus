/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string>
#include <memory>
#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "common/Utility.hpp"
#include "SshHook.hpp"

static void parseKeygenCLIOptions(int argc, char* argv[], bool * const overwrite);
static void parseCheckUserKeysCLIOptions(int argc, char* argv[]);

int main(int argc, char* argv[]) {
    try {
        if(argc < 2) {
            SARUS_THROW_ERROR("Failed to execute SSH hook."
                              " Bad number of CLI arguments.");
        }

        if(argv[1] == std::string{"keygen"}) {
            bool overwriteSshKeysIfExist = false;
            parseKeygenCLIOptions(argc, argv, &overwriteSshKeysIfExist);
            sarus::hooks::ssh::SshHook{}.generateSshKeys(overwriteSshKeysIfExist);
        }
        else if(argv[1] == std::string{"check-user-has-sshkeys"}) {
            parseCheckUserKeysCLIOptions(argc, argv);
            sarus::hooks::ssh::SshHook{}.checkUserHasSshKeys();
        }
        else if(argv[1] == std::string("start-ssh-daemon")) {
            sarus::hooks::ssh::SshHook{}.startSshDaemon();
        }
        else {
            auto message = boost::format("Failed to execute SSH hook. CLI argument %s is not supported.")
                % argv[1];
            SARUS_THROW_ERROR(message.str());
        }
    } catch(const sarus::common::Error& e) {
        sarus::common::Logger::getInstance().logErrorTrace(e, "ssh-hook");
        exit(EXIT_FAILURE);
    } catch(const std::exception& e) {
        sarus::common::Logger::getInstance().log(e.what(), "ssh-hook", sarus::common::LogLevel::ERROR);
        exit(EXIT_FAILURE);
    }

    return 0;
}

static void parseKeygenCLIOptions(int argc, char* argv[], bool * const overwrite) {
    *overwrite = false;
    if(argc > 2) {
        for (int i=2; i < argc; ++i) {
            if(argv[i] == std::string{"--overwrite"}) {
                *overwrite = true;
            }
            else if(argv[i] == std::string{"--verbose"}) {
                if (sarus::common::Logger::getInstance().getLevel() > sarus::common::LogLevel::INFO) {
                    sarus::common::Logger::getInstance().setLevel(sarus::common::LogLevel::INFO);
                }
            }
            else if(argv[i] == std::string{"--debug"}) {
                sarus::common::Logger::getInstance().setLevel(sarus::common::LogLevel::DEBUG);
            }
            else {
                auto message = boost::format("Failed to execute SSH hook. Invalid"
                                             "option %s for the 'keygen' command.") % argv[2];
                SARUS_THROW_ERROR(message.str());
            }
        }
    }
}

static void parseCheckUserKeysCLIOptions(int argc, char* argv[]){
    if(argc > 2) {
        for (int i=2; i < argc; ++i) {
            if(argv[i] == std::string{"--verbose"}) {
                if (sarus::common::Logger::getInstance().getLevel() > sarus::common::LogLevel::INFO) {
                    sarus::common::Logger::getInstance().setLevel(sarus::common::LogLevel::INFO);
                }
            }
            else if(argv[i] == std::string{"--debug"}) {
                sarus::common::Logger::getInstance().setLevel(sarus::common::LogLevel::DEBUG);
            }
        }
    }
}
