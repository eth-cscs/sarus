/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
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

std::shared_ptr<sarus::common::Config> Config::create(const boost::filesystem::path& sarusInstallationPrefixDir) {
    boost::filesystem::path configFilename =  sarusInstallationPrefixDir / "etc/sarus.json";
    boost::filesystem::path configSchemaFilename = sarusInstallationPrefixDir / "etc/sarus.schema.json";
    return Config::create(configFilename, configSchemaFilename);
}

std::shared_ptr<sarus::common::Config> Config::create(const boost::filesystem::path& configFilename,
                                                      const boost::filesystem::path& configSchemaFilename) {
    auto config = std::make_shared<Config>();
    auto schema = common::readJSON(configSchemaFilename);
    rapidjson::SchemaDocument schemaDoc(schema);
    config->json = common::readAndValidateJSON(configFilename, schemaDoc);
    return config;
}
}
}
