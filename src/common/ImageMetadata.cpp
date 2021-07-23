/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "ImageMetadata.hpp"

#include <fstream>
#include <sstream>
#include <tuple>
#include <algorithm>
#include <boost/format.hpp>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "common/Error.hpp"
#include "common/Utility.hpp"

namespace sarus {
namespace common {


ImageMetadata::ImageMetadata(const boost::filesystem::path& path, const common::UserIdentity& identity) {
    auto message = boost::format("Creating image metadata from file %s") % path;
    logMessage(message, LogLevel::INFO);

    auto rootIdentity = common::UserIdentity{};
    try {
        // switch to user identity to make sure we can access files on root_squashed filesystems
        common::setFilesystemUid(identity);

        auto json = common::readJSON(path);

        common::setFilesystemUid(rootIdentity);

        parseJSON(json);
    }
    catch (const common::Error& e) {
        common::setFilesystemUid(rootIdentity);
        auto message = boost::format("Error creating image metadata from file %s") % path;
        SARUS_RETHROW_ERROR(e, message.str());
    }

    logMessage("Successfully created image metadata", LogLevel::INFO);
}

ImageMetadata::ImageMetadata(const rapidjson::Value& metadata) {
    logMessage("Creating image metadata from JSON object", LogLevel::INFO);

    try {
        parseJSON(metadata);
    }
    catch (const common::Error& e) {
        auto message = boost::format("Error creating image metadata from JSON object ");
        SARUS_RETHROW_ERROR(e, message.str());
    }

    logMessage("Successfully created image metadata from JSON object", LogLevel::INFO);
}

void ImageMetadata::write(const boost::filesystem::path& path) const {
    auto message = boost::format("Writing image metadata file %s") % path;
    logMessage(message, LogLevel::INFO);

    auto json = rapidjson::Document(rapidjson::kObjectType);
    auto& allocator = json.GetAllocator();

    json.AddMember("Cmd", rapidjson::Value{rapidjson::kArrayType}, allocator);
    if (cmd) {
        for (auto* arg : *cmd ) {
            auto value = rapidjson::Value{arg, allocator};
            json["Cmd"].PushBack(value, allocator);
        }
    }

    json.AddMember("Entrypoint", rapidjson::Value{rapidjson::kArrayType}, allocator);
    if (entry) {
        for (auto* arg : *entry ) {
            auto value = rapidjson::Value{arg, allocator};
            json["Entrypoint"].PushBack(value, allocator);
        }
    }

    if (workdir) {
        json.AddMember("WorkingDir", rapidjson::Value{workdir->c_str(), allocator}, allocator);
    }
    else {
        json.AddMember("WorkingDir", rapidjson::Value{"", allocator}, allocator);
    }

    json.AddMember("Env", rapidjson::Value{rapidjson::kArrayType}, allocator);
    for(const auto& kv : env) {
        auto variable = kv.first + "=" + kv.second;
        json["Env"].PushBack(rapidjson::Value{variable.c_str(), allocator},
                             allocator);
    }

    common::writeJSON(json, path);

    logMessage("Successfully written image metadata file", LogLevel::INFO);
}

void ImageMetadata::parseJSON(const rapidjson::Value& json) {

    auto serialize = [](const rapidjson::Value& value) {
        rapidjson::StringBuffer buffer;
        buffer.Clear();
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        value.Accept(writer);
        return std::string(buffer.GetString());
    };

    if (json.HasMember("Cmd")
        && serialize(json["Cmd"]) != "null") {
        std::stringstream is(serialize(json["Cmd"]));
        cmd = common::CLIArguments{};
        is >> *cmd;
    }
    if (json.HasMember("Entrypoint")
        && serialize(json["Entrypoint"]) != "null") {
        std::stringstream is(serialize(json["Entrypoint"]));
        entry = common::CLIArguments{};
        is >> *entry;
    }
    if (json.HasMember("WorkingDir")
        && serialize(json["WorkingDir"]) != "null"
        && serialize(json["WorkingDir"]) != ""
        && serialize(json["WorkingDir"]) != "\"\"") {
        workdir = json["WorkingDir"].GetString();
    }
    if (json.HasMember("Env")) {
        for (const auto& v : json["Env"].GetArray()) {
            std::string variable = v.GetString();
            std::string key, value;
            std::tie(key, value) = common::parseEnvironmentVariable(variable);
            env[key] = value;
        }
    }
}

bool operator==(const ImageMetadata& lhs, const ImageMetadata& rhs) {
    return lhs.cmd == rhs.cmd
        && lhs.entry == rhs.entry
        && lhs.workdir == rhs.workdir
        && lhs.env == rhs.env;
}

}
}
