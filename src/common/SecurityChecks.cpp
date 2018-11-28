#include "SecurityChecks.hpp"

#include "common/Utility.hpp"


namespace sarus {
namespace common {

#define SKIP_SECURITY_CHECK_IF_NECESSARY(message) { \
    if(!Config::BuildTime{}.areRuntimeSecurityChecksEnabled) { \
        logMessage(message, logType::INFO); \
        return; \
    } \
}

void SecurityChecks::checkThatFileIsUntamperable(const boost::filesystem::path& file) const {
    SKIP_SECURITY_CHECK_IF_NECESSARY(
        boost::format(  "Skipping check that file %s is untamperable"
                        " (runtime security checks disabled)") % file);

    auto message = boost::format("Checking that file %s is untamperable") % file;
    logMessage(message, common::logType::INFO);

    uid_t uidParentDir; gid_t gidParentDir;
    uid_t uidFile; gid_t gidFile;

    try {
        std::tie(uidParentDir, gidParentDir) = common::getOwner(file.parent_path());
        std::tie(uidFile, gidFile) = common::getOwner(file);
    }
    catch(common::Error& e) {
        auto message = boost::format("Failed to check that file %s is untamperable") % file;
        SARUS_RETHROW_ERROR(e, message.str());
    }

    if(uidParentDir != 0 || gidParentDir != 0) {
        auto message = boost::format(   "Directory %s must be owned by root in order to prevent"
                                        "other users from tampering its contents. Found uid=%d, gid=%d.")
            % file.parent_path() % uidParentDir % gidParentDir;
        SARUS_THROW_ERROR(message.str());
    }

    if(uidFile != 0 || gidFile != 0) {
        auto message = boost::format(   "File %s must be owned by root in order to prevent"
                                        "other users from tampering its contents. Found uid=%d, gid=%d.")
            % file % uidFile % gidFile;
        SARUS_THROW_ERROR(message.str());
    }

    logMessage("Successfully checked that file is untamperable", common::logType::INFO);
}

void SecurityChecks::checkThatBinariesInSarusJsonAreUntamperable(const rapidjson::Document& json) const {
    SKIP_SECURITY_CHECK_IF_NECESSARY(   "Skipping check that binaries in sarus.json are"
                                        " untamperable (runtime security checks disabled)");

    checkThatFileIsUntamperable(json["mksquashfsPath"].GetString());
    checkThatFileIsUntamperable(json["runcPath"].GetString());
}

void SecurityChecks::checkThatOCIHooksAreUntamperable(const common::Config& config) const {
    SKIP_SECURITY_CHECK_IF_NECESSARY(   "Skipping check that OCI hooks are untamperable"
                                        " (runtime security checks disabled)");

    logMessage("Checking that OCI hooks are owned by root user", common::logType::INFO);

    if(!config.json.get().HasMember("OCIHooks")) {
        logMessage( "Successfully checked that OCI hooks are owned by root user."
                    " The configuration doesn't contain OCI hooks to check.", common::logType::INFO);
        return; // no hooks to check
    }

    checkThatOCIHooksAreUntamperableByType(config, "prestart");
    checkThatOCIHooksAreUntamperableByType(config, "poststart");
    checkThatOCIHooksAreUntamperableByType(config, "poststop");

    logMessage("Successfully checked that OCI hooks are owned by root user", common::logType::INFO);
}

void SecurityChecks::checkThatOCIHooksAreUntamperableByType(const common::Config& config, const std::string& hookType) const {
    logMessage(boost::format("Checking %s OCI hooks") % hookType, common::logType::DEBUG);

    const auto& json = config.json.get();
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
            checkThatFileIsUntamperable(path);
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
