#include "MountParser.hpp"

#include <limits>
#include <sstream>

#include <boost/filesystem.hpp>

#include "common/Error.hpp"
#include "cli/Utility.hpp"
#include "runtime/UserMount.hpp"
#include "runtime/SiteMount.hpp"

namespace sarus {
namespace cli {

MountParser::MountParser(bool isUserMount, const common::Config& conf)
    : isUserMount{isUserMount}
    , conf{&conf}
{
    if(isUserMount) {
        // Retrieve settings from Config struct
        if (conf.json.get().HasMember("userMounts")) {
            const rapidjson::Value& userMounts = conf.json.get()["userMounts"];
            for (const auto& value : userMounts["notAllowedPrefixesOfPath"].GetArray()) {
                validationSettings.destinationDisallowedWithPrefix.push_back(value.GetString());
            }
            for (const auto& value : userMounts["notAllowedPaths"].GetArray()) {
                validationSettings.destinationDisallowedExact.push_back(value.GetString());
            }
        }
        else {
            validationSettings.destinationDisallowedWithPrefix = {};
            validationSettings.destinationDisallowedExact = {};
        }

        validationSettings.allowedFlags.emplace("readonly",  true);
        validationSettings.allowedFlags.emplace("recursive", true);
        validationSettings.allowedFlags.emplace("private",   false);
        validationSettings.allowedFlags.emplace("rprivate",  false);
        validationSettings.allowedFlags.emplace("slave",     false);
        validationSettings.allowedFlags.emplace("rslave",    false);
    }
    else { // is site mount
        validationSettings.destinationDisallowedWithPrefix = {};
        validationSettings.destinationDisallowedExact = {};
        validationSettings.allowedFlags.emplace("readonly",  true);
        validationSettings.allowedFlags.emplace("recursive", true);
        validationSettings.allowedFlags.emplace("private",   true);
        validationSettings.allowedFlags.emplace("rprivate",  true);
        validationSettings.allowedFlags.emplace("slave",     true);
        validationSettings.allowedFlags.emplace("rslave",    true);
    }
}

/**
 * Parses a custom mount request into a Mount object.
 * The request comes in the form of a comma-separated list of key-value pairs.
 */
std::unique_ptr<runtime::Mount> MountParser::parseMountRequest(const std::unordered_map<std::string, std::string>& requestMap) {
    // The request has to specify the mount type
    if (requestMap.count("type") == 0) {
        auto message = boost::format("Failed to parse mount request %s. Mount request must specify type")
            % convertRequestMapToString(requestMap);
        SARUS_THROW_ERROR(message.str());
    }

    // Check that one and only one of the key variants for the destination is in use
    auto destination_count = requestMap.count("destination");
    auto dst_count = requestMap.count("dst");
    auto target_count = requestMap.count("target");
    if (destination_count == 0 && dst_count == 0 && target_count == 0) {
        auto message = boost::format("Failed to parse mount request %s. No destination specified for custom mount. "
                "Use one of 'destination', 'dst' or 'target'") % convertRequestMapToString(requestMap);
        SARUS_THROW_ERROR(message.str());
    }
    if (destination_count ? (dst_count || target_count) : (dst_count && target_count)) {
        auto message = boost::format("Failed to parse mount request %s. Multiple formats used to specify mount destination. "
                "Use only one of 'destination', 'dst' or 'target'") % convertRequestMapToString(requestMap);
        SARUS_THROW_ERROR(message.str());
    }

    // Parse sub-options for different mount types separately
    if (requestMap.at("type") == std::string{"bind"}) {
        try {
            return parseBindMountRequest(requestMap);
        }
        catch (common::Error& e) {
            auto message = boost::format("Failed to parse bind mount request %s") % convertRequestMapToString(requestMap);
            SARUS_RETHROW_ERROR(e, message.str());
        }
    }
    else {
        auto message = boost::format("Unrecognized type specified for mount request %s.") % convertRequestMapToString(requestMap);
        SARUS_THROW_ERROR(message.str());
    }

}

std::unique_ptr<runtime::Mount> MountParser::parseBindMountRequest(const std::unordered_map<std::string, std::string>& requestMap) {
    // Check that one and only one of the key variants for the source is in use
    // and retrieve its value
    std::string source;
    if (requestMap.count("source")) {
        if (requestMap.count("src")) {
            SARUS_THROW_ERROR("Multiple formats used to specify mount source. Use either 'source' or 'src'.");
        }
        source = requestMap.at("source");
    }
    else if (requestMap.count("src")) {
        source = requestMap.at("src");
    }
    else {
        SARUS_THROW_ERROR("No source specified for mount. Use either 'source' or 'src'.");
    }

    validateMountSource(source);

    // Retrieve mount destination value
    std::string destination;
    if (requestMap.count("destination")) {
        destination = requestMap.at("destination");
    }
    else if (requestMap.count("dst")) {
        destination = requestMap.at("dst");
    }
    else if (requestMap.count("target")) {
        destination = requestMap.at("target");
    }

    validateMountDestination(destination);

    // Create local copy of map and remove sub-options not expected as mount flags
    auto localMap = requestMap;
    localMap.erase("type");
    localMap.erase("source");
    localMap.erase("src");
    localMap.erase("destination");
    localMap.erase("dst");
    localMap.erase("target");

    // Parse the other sub-options into mount flags
    auto flags = convertBindMountFlags(localMap);

    if(isUserMount) {
        return std::unique_ptr<runtime::Mount>{new runtime::UserMount{source, destination, flags, *conf}};
    }
    else {
        return std::unique_ptr<runtime::Mount>{new runtime::SiteMount{source, destination, flags, *conf}};
    }
}

/*
 * Generates mount flags from a map that is expected to contain key-value
 * pairs representing auxiliary options for a custom bind mount.
 */
unsigned long MountParser::convertBindMountFlags(const std::unordered_map<std::string, std::string>& flagsMap) {
    unsigned long flags = 0;

    for (auto const& opt : flagsMap) {
        if (opt.first == std::string{"readonly"}) {
            if (validationSettings.allowedFlags.at("readonly") == false) {
                SARUS_THROW_ERROR("Option 'readonly' is not allowed for this mount.");
            }
            flags |= MS_RDONLY;
        }
        else if (opt.first == std::string{"bind-propagation"}) {
            if (opt.second == std::string{"recursive"}) {
                if (validationSettings.allowedFlags.at("recursive") == false) {
                    SARUS_THROW_ERROR("Option 'bind-propagation=recursive' is not allowed for this mount.");
                }
                flags |= MS_REC;
            }
            else if (opt.second == std::string{"slave"}) {
                if (validationSettings.allowedFlags.at("slave") == false) {
                    SARUS_THROW_ERROR("Option 'bind-propagation=slave' is not allowed for this mount.");
                }
                flags |= MS_SLAVE;
            }
            else if (opt.second == std::string{"rslave"}) {
                if (validationSettings.allowedFlags.at("rslave") == false) {
                    SARUS_THROW_ERROR("Option 'bind-propagation=rslave' is not allowed for this mount.");
                }
                flags |= MS_SLAVE;
                flags |= MS_REC;
            }
            else if (opt.second == std::string{"private"}) {
                if (validationSettings.allowedFlags.at("private") == false) {
                    SARUS_THROW_ERROR("Option 'bind-propagation=private' is not allowed for this mount.");
                }
                flags |= MS_PRIVATE;
            }
            else if (opt.second == std::string{"rprivate"}) {
                if (validationSettings.allowedFlags.at("rprivate") == false) {
                    SARUS_THROW_ERROR("Option 'bind-propagation=rprivate' is not allowed for this mount.");
                }
                flags |= MS_PRIVATE;
                flags |= MS_REC;
            }
            else {
                auto message = boost::format("Unrecognized value specified for bind propagation: %s.")
                        % opt.second;
                SARUS_THROW_ERROR(message.str());
            }
        }
        else {
            auto message = boost::format("Unrecognized option specified for bind mount: %s.")
                    % opt.first;
            SARUS_THROW_ERROR(message.str());
        }
    }

    return flags;
}

void MountParser::validateMountSource(const std::string& source_str) {
    boost::filesystem::path source(source_str);

    if (source == "") {
        SARUS_THROW_ERROR("Invalid mount source (empty).");
    }

    if (source.is_relative()) {
        SARUS_THROW_ERROR("Only absolute paths are accepted as custom mount sources.");
    }

    for (const auto& disallowed_prefix : validationSettings.sourceDisallowedWithPrefix) {
        if (source.string().find(disallowed_prefix) == 0) {
            auto message = boost::format("Custom mounts are not allowed from %s and its subdirectories.")
                    % disallowed_prefix;
            SARUS_THROW_ERROR(message.str());
        }
    }
    for (const auto& disallowed : validationSettings.sourceDisallowedExact) {
        if (source.string() == disallowed) {
            auto message = boost::format("Custom mounts are not allowed from %s.") % disallowed;
            SARUS_THROW_ERROR(message.str());
        }
    }
}

void MountParser::validateMountDestination( const std::string& destination_str) {
    boost::filesystem::path destination(destination_str);

    if (destination == "") {
        SARUS_THROW_ERROR("Invalid mount destination (empty).");
    }

    if (destination.is_relative()) {
        SARUS_THROW_ERROR("Only absolute paths are accepted as custom mount destinations.");
    }

    for (const auto& disallowed_prefix : validationSettings.destinationDisallowedWithPrefix) {
        if (destination.string().find(disallowed_prefix) == 0) {
            auto message = boost::format("Custom mounts are not allowed to %s and its subdirectories.")
                    % disallowed_prefix;
            SARUS_THROW_ERROR(message.str());
        }
    }
    for (const auto& disallowed : validationSettings.destinationDisallowedExact) {
        if (destination.string() == disallowed) {
            auto message = boost::format("Custom mounts are not allowed to %s.") % disallowed;
            SARUS_THROW_ERROR(message.str());
        }
    }
}

// used to produce log messages
std::string MountParser::convertRequestMapToString(const std::unordered_map<std::string, std::string>& map) const {
    std::stringstream s{};
    s << "{";
    bool isFirstValue = true;
    for(const auto& value : map) {
        if(!isFirstValue) {
            s << ", ";
        }
        else {
            isFirstValue = false;
        }
        s << "{" << value.first << ", " << value.second << "}";
    }
    s << "}";
    return s.str();
}

} // namespace
} // namespace
