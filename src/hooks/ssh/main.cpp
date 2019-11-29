/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string>
#include <boost/format.hpp>

#include "common/Config.hpp"
#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "common/SecurityChecks.hpp"
#include "common/Utility.hpp"
#include "hooks/common/Utility.hpp"
#include "SshHook.hpp"

int main(int argc, char* argv[]) {
    try {
        if(argc < 2) {
            SARUS_THROW_ERROR("Failed to execute SSH hook."
                              " Bad number of CLI arguments.");
        }

        // Create Config
        boost::filesystem::path sarusInstallationPrefixDir = sarus::common::getEnvironmentVariable("SARUS_PREFIX_DIR");
        auto config = sarus::common::Config::create(sarusInstallationPrefixDir);
        sarus::common::SecurityChecks{config}.runSecurityChecks(sarusInstallationPrefixDir);

        if(argv[1] == std::string{"keygen"}) {
            bool overwriteSshKeysIfExist = false;
            if(argc > 2) {
                if(argv[2] == std::string{"--overwrite"}) {
                    overwriteSshKeysIfExist = true;
                }
                else {
                    auto message = boost::format("Failed to execute SSH hook. Invalid"
                                                 "option %s for the 'keygen' command.") % argv[2];
                    SARUS_THROW_ERROR(message.str());
                }
            }
            sarus::hooks::ssh::SshHook{config}.generateSshKeys(overwriteSshKeysIfExist);
        }
        else if(argv[1] == std::string{"check-localrepository-has-sshkeys"}) {
            sarus::hooks::ssh::SshHook{config}.checkLocalRepositoryHasSshKeys();
        }
        else if(argv[1] == std::string("start-sshd")) {
            sarus::hooks::ssh::SshHook{config}.startSshd();
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
