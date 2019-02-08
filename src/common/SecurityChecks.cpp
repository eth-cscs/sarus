#include "SecurityChecks.hpp"

#include "common/Utility.hpp"


namespace sarus {
namespace common {

#define SKIP_SECURITY_CHECK_IF_NECESSARY(message) { \
    if(!config->buildTime.areRuntimeSecurityChecksEnabled) { \
        logMessage(message, logType::INFO); \
        return; \
    } \
}

SecurityChecks::SecurityChecks(std::shared_ptr<const common::Config> config)
    : config{std::move(config)}
{}

void SecurityChecks::checkThatPathIsUntamperable(const boost::filesystem::path& path) const {
    SKIP_SECURITY_CHECK_IF_NECESSARY(
        boost::format(  "Skipping check that path %s is untamperable"
                        " (runtime security checks disabled)") % path);

    auto message = boost::format("Checking that path %s is untamperable") % path;
    logMessage(message, common::logType::INFO);

    if(path.has_parent_path()) {
        checkThatPathIsRootOwned(path.parent_path());
        checkThatPathIsNotGroupWritableOrWorldWritable(path.parent_path());
    }

    checkThatPathIsRootOwned(path);
    checkThatPathIsNotGroupWritableOrWorldWritable(path);

    // recursively check that subfolders/subfiles are untamperable (if directory)
    if(boost::filesystem::is_directory(path)) {
        for(boost::filesystem::directory_iterator entry{path};
            entry != boost::filesystem::directory_iterator{};
            ++entry) {
            checkThatPathIsUntamperable(entry->path());
        }
    }

    logMessage("Successfully checked that path is untamperable", common::logType::INFO);
}

void SecurityChecks::checkThatBinariesInSarusJsonAreUntamperable(const rapidjson::Document& json) const {
    SKIP_SECURITY_CHECK_IF_NECESSARY(   "Skipping check that binaries in sarus.json are"
                                        " untamperable (runtime security checks disabled)");

    checkThatPathIsUntamperable(json["mksquashfsPath"].GetString());
    checkThatPathIsUntamperable(json["runcPath"].GetString());
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

    if(uid != 0 || gid != 0) {
        auto message = boost::format(   "Path %s must be owned by root in order to prevent"
                                        "other users from tampering its contents. Found uid=%d, gid=%d.")
            % path % uid % gid;
        SARUS_THROW_ERROR(message.str());
    }
}

void SecurityChecks::checkThatPathIsNotGroupWritableOrWorldWritable(const boost::filesystem::path& path) const {
    auto status = boost::filesystem::status(path);
    auto isGroupWritable = status.permissions() & (1 << 4);
    auto isWorldWritable = status.permissions() & (1 << 1);
    if(isGroupWritable || isWorldWritable) {
        auto message = boost::format(   "Path %s cannot be group- or world-writable in order"
                                        " to prevent other users from tampering its contents.")
            % path;
        SARUS_THROW_ERROR(message.str());
    }
}

void SecurityChecks::checkThatOCIHooksAreUntamperable() const {
    SKIP_SECURITY_CHECK_IF_NECESSARY(   "Skipping check that OCI hooks are untamperable"
                                        " (runtime security checks disabled)");

    logMessage("Checking that OCI hooks are owned by root user", common::logType::INFO);

    if(!config->json.get().HasMember("OCIHooks")) {
        logMessage( "Successfully checked that OCI hooks are owned by root user."
                    " The configuration doesn't contain OCI hooks to check.", common::logType::INFO);
        return; // no hooks to check
    }

    checkThatOCIHooksAreUntamperableByType("prestart");
    checkThatOCIHooksAreUntamperableByType("poststart");
    checkThatOCIHooksAreUntamperableByType("poststop");

    logMessage("Successfully checked that OCI hooks are owned by root user", common::logType::INFO);
}

void SecurityChecks::checkThatOCIHooksAreUntamperableByType(const std::string& hookType) const {
    logMessage(boost::format("Checking %s OCI hooks") % hookType, common::logType::DEBUG);

    const auto& json = config->json.get();
    if(!json["OCIHooks"].HasMember(hookType.c_str())) {
        logMessage(boost::format(   "Successfully checked %s OCI hooks."
                                    " The configuration doesn't contain %s OCI hooks to check.") % hookType % hookType,
                common::logType::DEBUG);
        return;
    }

    for(const auto& hook : json["OCIHooks"][hookType.c_str()].GetArray()) {
        auto path = boost::filesystem::path{ hook["path"].GetString() };

        logMessage( boost::format("Checking OCI hook %s") % path,
                    common::logType::DEBUG);

        try {
            checkThatPathIsUntamperable(path);
        }
        catch(common::Error& e) {
            auto message = boost::format("Failed to check that OCI hook %s is untamperable") % path;
            SARUS_RETHROW_ERROR(e, message.str());
        }

        logMessage( boost::format("Successfully checked OCI hook %s") % path,
                    common::logType::DEBUG);
    }

    logMessage( boost::format("Successfully checked %s OCI hooks") % hookType,
                common::logType::DEBUG);
}

}} // namespace
