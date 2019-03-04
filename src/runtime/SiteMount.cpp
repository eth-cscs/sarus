/**
 *  @brief Source file for site-requested mounts.
 */

#include "SiteMount.hpp"

#include <boost/format.hpp>

#include "common/Utility.hpp"
#include "common/Logger.hpp"
#include "runtime/mount_utilities.hpp"

namespace sarus {
namespace runtime {

SiteMount::SiteMount(   const boost::filesystem::path& source,
                        const boost::filesystem::path& destination,
                        const size_t mountFlags,
                        std::shared_ptr<const common::Config> config)
    : source{source}
    , destination{destination}
    , mountFlags{mountFlags}
    , config{std::move(config)}
{}

void SiteMount::performMount() const {
    validateMountSource(source);

    auto realpathOfSource = std::shared_ptr<char>{realpath(source.string().c_str(), NULL), free};
    if (!realpathOfSource) {
        auto message = boost::format("Failed to find real path for site-requested mount source: %s") % source;
        SARUS_THROW_ERROR(message.str());
    }

    auto rootfsDir = boost::filesystem::path{config->json.get()["OCIBundleDir"].GetString()} /
        config->json.get()["rootfsFolder"].GetString();
    auto destinationReal = common::realpathWithinRootfs(rootfsDir, destination);

    validateMountDestination(destinationReal, *config);

    if(boost::filesystem::is_directory(realpathOfSource.get())) {
        common::createFoldersIfNecessary(destinationReal, config->userIdentity.uid, config->userIdentity.gid);
    }
    else {
        common::createFileIfNecessary(destinationReal, config->userIdentity.uid, config->userIdentity.gid);
    }

    try {
        bindMount(realpathOfSource.get(), destinationReal, mountFlags);
    } catch(common::Error& e) {
        auto message = boost::format("Failed bind mount from %s to %s") % source % destination;
        SARUS_RETHROW_ERROR(e, message.str());
    }
}

} // namespace
} // namespace
