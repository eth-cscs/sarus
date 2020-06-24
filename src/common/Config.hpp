/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_common_Config_hpp
#define sarus_common_Config_hpp

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "common/CLIArguments.hpp"
#include "common/ImageID.hpp"
#include "common/UserIdentity.hpp"
#include "common/Logger.hpp"
#include "runtime/Mount.hpp"


namespace sarus {
namespace common {

class Config {
    public:
        Config() = default;
        Config(const boost::filesystem::path& configFilename,
               const boost::filesystem::path& configSchemaFilename);
        Config(const boost::filesystem::path& sarusInstallationPrefixDir);

        struct BuildTime {
            BuildTime();
            std::string version;
            boost::filesystem::path localRepositoryFolder = ".sarus";
            boost::filesystem::path dropbearmultiBuildArtifact;
        };

        struct Directories {
            void initialize(bool useCentralizedRepository, const common::Config& config);
            boost::filesystem::path repository;
            boost::filesystem::path cache;
            boost::filesystem::path temp;
            std::string tempFromCLI;
            boost::filesystem::path images;
        };

        struct Authentication {
            bool isAuthenticationNeeded = false;
            std::string username;
            std::string password;
        };

        struct CommandRun {
            std::unordered_map<std::string, std::string> hostEnvironment;
            std::unordered_map<std::string, std::string> bundleAnnotations;
            std::string cpusAllowedList;
            std::vector<std::string> userMounts;
            std::vector<std::shared_ptr<runtime::Mount>> mounts;
            boost::optional<boost::filesystem::path> workdir;
            boost::optional<CLIArguments> entrypoint;
            CLIArguments execArgs;
            bool allocatePseudoTTY = false;
            bool addInitProcess = false;
            bool useMPI = false;
            bool enableGlibcReplacement = false;
            bool enableSSH = false;
        };

        boost::filesystem::path getImageFile() const;
        boost::filesystem::path getMetadataFileOfImage() const;

        BuildTime buildTime;
        common::ImageID imageID;
        Directories directories;
        rapidjson::Document json{ rapidjson::kObjectType };
        UserIdentity userIdentity;
        Authentication authentication;
        CommandRun commandRun;

        boost::filesystem::path archivePath; // for CommandLoad

        bool useCentralizedRepository = false;

        std::chrono::high_resolution_clock::time_point program_start; // for time measurement
};
}
}

#endif
