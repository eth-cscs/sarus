/**
 *  @brief Source file for user-requested mounts.
 */

#include "UserMount.hpp"

#include <sys/stat.h>
#include <grp.h>
#include <errno.h>

#include "common/Utility.hpp"
#include "runtime/mount_utilities.hpp"


namespace sarus {
namespace runtime {

UserMount::UserMount(   const boost::filesystem::path& source,
                        const boost::filesystem::path& destination,
                        const size_t mountFlags,
                        const common::Config& config)
    : source{source}
    , destination{destination}
    , mountFlags{mountFlags}
    , config{&config}
{}

void UserMount::performMount() const {
    common::logMessage(boost::format("Performing User Mount: source = %s; target = %s; mount flags = %d")
        % source.string() % destination.string() % mountFlags, common::logType::DEBUG);

    // backup user + group ids
    auto backupUid = geteuid();
    auto backupGid = getegid();

    auto numOfSupplementaryGids = getgroups(0, NULL);
    if (numOfSupplementaryGids == -1) {
        auto message = boost::format("Failed to getgroups: %s") % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    auto backupSupplementaryGids = std::vector<gid_t>(numOfSupplementaryGids);
    if(backupSupplementaryGids.size() > 0) {
        auto ret = getgroups(numOfSupplementaryGids, backupSupplementaryGids.data());
        if (ret <= 0) {
            auto message = boost::format("Failed to getgroups: %s") % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }
    }

    // switch to user's identity to check that the mount's source is actually accessible by the user
    if (setgroups(config->userIdentity.supplementaryGids.size(), config->userIdentity.supplementaryGids.data()) != 0) {
        SARUS_THROW_ERROR("Failed to assume end-user auxiliary gids");
    }

    if (setegid(config->userIdentity.gid) != 0) {
        SARUS_THROW_ERROR("Failed to assume end-user gid");
    }

    if (seteuid(config->userIdentity.uid) != 0) {
        SARUS_THROW_ERROR("Failed to assume end-user uid");
    }

    validateMountSource(source);

    auto realpathOfSource = std::shared_ptr<char>{realpath(source.string().c_str(), NULL), free};
    if (!realpathOfSource) {
        auto message = boost::format("Failed to find real path for user-requested mount source: %s") % source;
        SARUS_THROW_ERROR(message.str());
    }

    // switch to backed up identity
    if (seteuid(backupUid) != 0) {
        SARUS_THROW_ERROR("Failed to re-assume original user effective uid");
    }

    if (setegid(backupGid) != 0) {
        SARUS_THROW_ERROR("Failed to re-assume original user effective gid");
    }

    if (setgroups(backupSupplementaryGids.size(), backupSupplementaryGids.data()) != 0) {
        SARUS_THROW_ERROR("Failed to re-assume original user auxiliary gids");
    }

    validateMountDestination(destination, *config);

    auto rootfsDir = boost::filesystem::path{ config->json.get()["OCIBundleDir"].GetString() }
        / config->json.get()["rootfsFolder"].GetString();
    auto destinationReal = common::realpathWithinRootfs(rootfsDir, destination);

    if(boost::filesystem::is_directory(realpathOfSource.get())) {
        common::createFoldersIfNecessary(rootfsDir / destination, config->userIdentity.uid, config->userIdentity.gid);
    }
    else {
        common::createFileIfNecessary(rootfsDir / destination, config->userIdentity.uid, config->userIdentity.gid);
    }

    try {
        bindMount(realpathOfSource.get(), destinationReal, mountFlags);
    }
    catch(common::Error& e) {
        auto message = boost::format("Failed user requested bind mount from %s to %s") % source % destination;
        SARUS_THROW_ERROR(message.str());
    }
}

} // namespace
} // namespace
