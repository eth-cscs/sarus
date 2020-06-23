/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <algorithm>
#include <fstream>
#include <sys/stat.h>

#include "common/Config.hpp"
#include "common/Utility.hpp"


namespace sarus {
namespace common {

Config::Config(const boost::filesystem::path& sarusInstallationPrefixDir)
    : Config{sarusInstallationPrefixDir / "etc/sarus.json", sarusInstallationPrefixDir / "etc/sarus.schema.json"}
{}

Config::Config(const boost::filesystem::path& configFilename,
               const boost::filesystem::path& configSchemaFilename)
    : json{ common::readAndValidateJSON(configFilename, configSchemaFilename) }
{}

void Config::Directories::initialize(bool useCentralizedRepository, const common::Config& config) {
    if (useCentralizedRepository) {
        common::logMessage( boost::format("initializing CLI config's directories for centralized repository"),
                            common::LogLevel::DEBUG);
        repository = common::getCentralizedRepositoryDirectory(config);
        images = repository / "images";
    }
    else {
        common::logMessage( boost::format("initializing CLI config's directories for local repository"),
                            common::LogLevel::DEBUG);
        repository = common::getLocalRepositoryDirectory(config);
        images = repository / "images";
    }

    cache = repository / "cache";

    bool tempDirWasSpecifiedThroughCLI = tempFromCLI != "";
    if(tempDirWasSpecifiedThroughCLI) {
        temp = boost::filesystem::absolute(tempFromCLI);
    }
    else {
        temp = boost::filesystem::path(config.json["tempDir"].GetString());
    }
}

boost::filesystem::path Config::getImageFile() const {
    auto key = imageID.getUniqueKey();
    auto file = boost::filesystem::path(directories.images.string() + "/" + key + ".squashfs");
    return file;
}

boost::filesystem::path Config::getMetadataFileOfImage() const {
    auto key = imageID.getUniqueKey();
    auto file = boost::filesystem::path(directories.images.string() + "/" + key + ".meta");
    return file;
}

}} // namespaces
