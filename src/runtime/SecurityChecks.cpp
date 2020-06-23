/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "SecurityChecks.hpp"

#include "common/Utility.hpp"
#include "runtime/Utility.hpp"
#include "runtime/OCIHooksFactory.hpp"

namespace sarus {
namespace runtime {

SecurityChecks::SecurityChecks(std::shared_ptr<const common::Config> config)
    : config{std::move(config)}
{}

void SecurityChecks::checkThatPathIsUntamperable(const boost::filesystem::path& path) const {
    auto message = boost::format("Checking that path %s is untamperable") % path;
    utility::logMessage(message, common::LogLevel::INFO);

    // check that path exists
    if(!boost::filesystem::exists(path)) {
        message = boost::format("Path %s does not exist, skipping") % path;
        utility::logMessage(message, common::LogLevel::INFO);
        return;
    }

    // check that parent paths are untamperable
    auto rootPath = path.root_path();
    auto parentPath = path;
    do {
        checkThatPathIsRootOwned(parentPath);
        checkThatPathIsNotGroupWritableOrWorldWritable(parentPath);
        parentPath = parentPath.parent_path();
    } while(boost::filesystem::exists(parentPath) && parentPath != rootPath);

    // if directory, check that subpaths are untamperable
    if(boost::filesystem::is_directory(path)) {
        for(boost::filesystem::recursive_directory_iterator entry{path};
              entry != boost::filesystem::recursive_directory_iterator{};
              ++entry) {
            checkThatPathIsRootOwned(entry->path());
            checkThatPathIsNotGroupWritableOrWorldWritable(entry->path());
        }
    }

    message = boost::format("Successfully checked that path %s is untamperable") % path;
    utility::logMessage(message, common::LogLevel::INFO);
}

void SecurityChecks::checkThatBinariesInSarusJsonAreUntamperable() const {
    checkThatPathIsUntamperable(config->json["mksquashfsPath"].GetString());
    checkThatPathIsUntamperable(config->json["initPath"].GetString());
    checkThatPathIsUntamperable(config->json["runcPath"].GetString());
}

void SecurityChecks::checkThatPathIsRootOwned(const boost::filesystem::path& path) const {
    uid_t uid; gid_t gid;
    try {
        std::tie(uid, gid) = common::getOwner(path);
    }
    catch(common::Error& e) {
        auto message = boost::format("Failed to check that path %s is untamperable") % path;
        SARUS_RETHROW_ERROR(e, message.str());
    }

    if(uid != 0) {
        auto message = boost::format("Path %s must be owned by root in order to prevent"
                                     " other users from tampering its contents. Found uid=%d, gid=%d.")
            % path % uid % gid;
        SARUS_THROW_ERROR(message.str());
    }
}

void SecurityChecks::checkThatPathIsNotGroupWritableOrWorldWritable(const boost::filesystem::path& path) const {
    auto status = boost::filesystem::status(path);
    auto isGroupWritable = status.permissions() & (1 << 4);
    auto isWorldWritable = status.permissions() & (1 << 1);
    if(isGroupWritable || isWorldWritable) {
        auto message = boost::format("Path %s cannot be group- or world-writable in order"
                                     " to prevent other users from tampering its contents.")
            % path;
        SARUS_THROW_ERROR(message.str());
    }
}

void SecurityChecks::checkThatOCIHooksAreUntamperable() const {
    utility::logMessage("Checking that OCI hooks are owned by root user", common::LogLevel::INFO);

    if(!config->json.HasMember("hooksDir")) {
        utility::logMessage("Successfully checked that OCI hooks are owned by root user."
                            " The configuration doesn't contain OCI hooks to check.",
                            common::LogLevel::INFO);
        return; // no hooks to check
    }

    auto hooksDir = boost::filesystem::path{ config->json["hooksDir"].GetString() };
    auto schemaFile = boost::filesystem::path{ config->json["prefixDir"].GetString() } / "etc/hook.schema.json";

    for(const auto& hook : runtime::OCIHooksFactory{}.createHooks(hooksDir, schemaFile)) {
        checkThatPathIsUntamperable(hook.jsonFile);
        auto hookBinary = boost::filesystem::path{ hook.jsonHook["path"].GetString() };
        checkThatPathIsUntamperable(hookBinary);
    }

    utility::logMessage("Successfully checked that OCI hooks are owned by root user", common::LogLevel::INFO);
}

void SecurityChecks::runSecurityChecks(const boost::filesystem::path& sarusInstallationPrefixDir) const {
    // Sarus config file must always be untamperable
    boost::filesystem::path configFilename =  sarusInstallationPrefixDir / "etc/sarus.json";
    boost::filesystem::path configSchemaFilename = sarusInstallationPrefixDir / "etc/sarus.schema.json";

    // "Weakly" check that sarus.json and sarus.schema.json are untamperable:
    // check that the two files are root-owned and only root-writable, but ignore the ownership
    // and permissions of the ancestor directories.
    //
    // IMPORTANT!!!
    // sarus.json and sarus.schema.json must be processed in this order:
    // 1. Read the contents of sarus.json and sarus.schema.json (before calling this function).
    // 2. Check that sarus.json and sarus.schema.json are root-owned and only root-writable.
    //
    // Inverting the order of those two operations would result in a security hazard, because
    // an attacker could replace the contents of sarus.json and sarus.schema.json in the time
    // between the security check and the read operation.
    checkThatPathIsRootOwned(configFilename);
    checkThatPathIsNotGroupWritableOrWorldWritable(configFilename);
    checkThatPathIsRootOwned(configSchemaFilename);
    checkThatPathIsNotGroupWritableOrWorldWritable(configSchemaFilename);

    // The rest of the checks depend on user configuration
    if(!config->json["securityChecks"].GetBool()) {
        auto message =  "Skipping security checks (disabled in the sarus.json config file)";
        utility::logMessage(message, common::LogLevel::INFO);
    }
    else {
        checkThatBinariesInSarusJsonAreUntamperable();
        checkThatOCIHooksAreUntamperable();
        checkThatPathIsUntamperable(boost::filesystem::path{config->json["OCIBundleDir"].GetString()});
        checkThatPathIsUntamperable(boost::filesystem::path{config->json["prefixDir"].GetString() + std::string{"/openssh"}});
    }
}

}} // namespace
