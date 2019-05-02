#include "common/Config.hpp"

#include <algorithm>
#include <fstream>
#include <sys/stat.h>

#include "common/Utility.hpp"
#include "common/SecurityChecks.hpp"


namespace sarus {
namespace common {

void Config::Directories::initialize(bool useCentralizedRepository, const common::Config& config) {
    if(useCentralizedRepository) {
        common::logMessage( boost::format("initializing CLI config's directories for centralized repository"),
                            common::logType::DEBUG);
        repository = common::getCentralizedRepositoryDirectory(config);
        images = repository / "images";
    }
    else {
        common::logMessage( boost::format("initializing CLI config's directories for local repository"),
                            common::logType::DEBUG);
        repository = common::getLocalRepositoryDirectory(config);
        images = repository / "images";
    }

    cache = repository / "cache";

    bool tempDirWasSpecifiedThroughCLI = tempFromCLI != "";
    if(tempDirWasSpecifiedThroughCLI) {
        temp = boost::filesystem::absolute(tempFromCLI);
    }
    else {
        temp = boost::filesystem::path(config.json.get()["tempDir"].GetString());
    }
}

Config::JSON::JSON()
    : file{new rapidjson::Document{rapidjson::kObjectType}}
{}

void Config::JSON::initialize(const boost::filesystem::path& configFilename, const boost::filesystem::path& schemaFilename) {
    auto securityChecks = SecurityChecks{std::make_shared<common::Config>()};
    securityChecks.checkThatPathIsUntamperable(configFilename);

    auto schema = common::readJSON(schemaFilename);
    rapidjson::SchemaDocument schemaDoc(schema); // convert to SchemaDocument
    auto json = common::readAndValidateJSON(configFilename, schemaDoc);
    file = std::make_shared<rapidjson::Document>(std::move(json));

    securityChecks.checkThatBinariesInSarusJsonAreUntamperable(*file);
}

Config::UserIdentity::UserIdentity() {
    // store uid + gid
    uid = getuid();
    gid = getgid();

    // store supplementary gids (if any)
    auto numOfSupplementaryGids = getgroups(0, NULL);
    if(numOfSupplementaryGids > 0) {
        supplementaryGids = std::vector<gid_t>(numOfSupplementaryGids);
        if (getgroups(supplementaryGids.size(), supplementaryGids.data()) == -1) {
            auto message = boost::format("Failed to retrieve supplementary group list: %s") % strerror(errno);
            SARUS_THROW_ERROR(message.str());
        }
    }

    // According to the getgroups(2) manpage, it is unspecified whether the effective
    // group ID of the calling process is included in the returned supplementary gid list.
    // Ensure the effective gid is included for consistency with host identification tools.
    auto egid = getegid();
    if (std::find(supplementaryGids.begin(), supplementaryGids.end(), egid) == supplementaryGids.end()) {
        supplementaryGids.push_back(egid);
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

}
}
