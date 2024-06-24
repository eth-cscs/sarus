/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <algorithm>
#include <fstream>
#include <sys/stat.h>

#include "common/Config.hpp"
#include "libsarus/Error.hpp"
#include "libsarus/PasswdDB.hpp"
#include "libsarus/Utility.hpp"


namespace sarus {
namespace common {

Config::Config(const boost::filesystem::path& sarusInstallationPrefixDir)
    : Config{sarusInstallationPrefixDir / "etc/sarus.json", sarusInstallationPrefixDir / "etc/sarus.schema.json"}
{}

Config::Config(const boost::filesystem::path& configFilename,
               const boost::filesystem::path& configSchemaFilename)
    : json{ libsarus::readAndValidateJSON(configFilename, configSchemaFilename) }
{}

void Config::Directories::initialize(bool useCentralizedRepository, const common::Config& config) {
    if (useCentralizedRepository) {
        libsarus::logMessage( boost::format("initializing CLI config's directories for centralized repository"),
                            libsarus::LogLevel::DEBUG);
        repository = config.getCentralizedRepositoryDirectory();
        images = repository / "images";
        libsarus::createFoldersIfNecessary(images,config.userIdentity.uid, config.userIdentity.gid);
    }
    else {
        libsarus::logMessage( boost::format("initializing CLI config's directories for local repository"),
                            libsarus::LogLevel::DEBUG);
        repository = config.getLocalRepositoryDirectory();
        images = repository / "images";
        libsarus::createFoldersIfNecessary(images, config.userIdentity.uid, config.userIdentity.gid);
    }

    cache = repository / "cache";
    libsarus::createFoldersIfNecessary(cache, config.userIdentity.uid, config.userIdentity.gid);
    libsarus::createFoldersIfNecessary(cache / "ociImages", config.userIdentity.uid, config.userIdentity.gid);
    libsarus::createFoldersIfNecessary(cache / "blobs", config.userIdentity.uid, config.userIdentity.gid);

    bool tempDirWasSpecifiedThroughCLI = !tempFromCLI.empty();
    if(tempDirWasSpecifiedThroughCLI) {
        temp = boost::filesystem::absolute(tempFromCLI);
    }
    else {
        temp = boost::filesystem::path(config.json["tempDir"].GetString());
    }
    if (!boost::filesystem::is_directory(temp)) {
        auto message = boost::format("Invalid temporary directory %s") % temp;
        logMessage(message, libsarus::LogLevel::GENERAL, std::cerr);
        SARUS_THROW_ERROR(message.str(), libsarus::LogLevel::INFO);
    }
}

boost::filesystem::path Config::getImageFile() const {
    auto key = imageReference.getUniqueKey();
    auto file = boost::filesystem::path(directories.images.string() + "/" + key + ".squashfs");
    return file;
}

boost::filesystem::path Config::getMetadataFileOfImage() const {
    auto key = imageReference.getUniqueKey();
    auto file = boost::filesystem::path(directories.images.string() + "/" + key + ".meta");
    return file;
}

bool Config::isCentralizedRepositoryEnabled() const {
    // centralized repository is enabled when a directory is specified
    return json.HasMember("centralizedRepositoryDir");
}

boost::filesystem::path Config::getCentralizedRepositoryDirectory() const {
    if(!isCentralizedRepositoryEnabled()) {
        SARUS_THROW_ERROR("failed to retrieve directory of centralized repository"
                            " because such feature is disabled. Please ask your system"
                            " administrator to enable the central read-only repository.");
    }
    return json["centralizedRepositoryDir"].GetString();
}

boost::filesystem::path Config::getLocalRepositoryDirectory() const {
    auto baseDir = boost::filesystem::path{ json["localRepositoryBaseDir"].GetString() };
    auto passwdFile = boost::filesystem::path{ json["prefixDir"].GetString() } / "etc/passwd";
    auto username = libsarus::PasswdDB{passwdFile}.getUsername(userIdentity.uid);
    return baseDir / username / common::Config::BuildTime{}.localRepositoryFolder;
}

boost::filesystem::path Config::getRootfsDirectory() const {
    return boost::filesystem::path{ json["OCIBundleDir"].GetString() }
           / json["rootfsFolder"].GetString();
}

}} // namespaces
