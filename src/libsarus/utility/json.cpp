/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "json.hpp"

#include <fstream>
#include <sstream>

#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>
#include <rapidjson/writer.h>

#include "libsarus/Error.hpp"
#include "libsarus/utility/filesystem.hpp"
#include "libsarus/utility/logging.hpp"

/**
 * Utility functions for JSON operations
 */

namespace libsarus {
namespace json {

rapidjson::Document parseStream(std::istream& is) {
    auto json = rapidjson::Document{};

    try {
        rapidjson::IStreamWrapper isw(is);
        json.ParseStream(isw);
    }
    catch (const std::exception& e) {
        SARUS_RETHROW_ERROR(e, "Error parsing JSON stream");
    }

    return json;
}

rapidjson::Document parse(const std::string& string) {
    std::istringstream iss(string);
    auto json = parseStream(iss);
    if (json.HasParseError()) {
        auto message = boost::format(
            "Error parsing JSON string:\n'%s'\nInput data is not valid JSON\n"
            "Error(offset %u): %s")
            % string
            % static_cast<unsigned>(json.GetErrorOffset())
            % rapidjson::GetParseError_En(json.GetParseError());
        SARUS_THROW_ERROR(message.str());
    }
    return json;
}

rapidjson::Document read(const boost::filesystem::path& filename) {
    std::ifstream ifs(filename.string());
    auto json = parseStream(ifs);
    if (json.HasParseError()) {
        auto message = boost::format(
            "Error parsing JSON file %s. Input data is not valid JSON\n"
            "Error(offset %u): %s")
            % filename
            % static_cast<unsigned>(json.GetErrorOffset())
            % rapidjson::GetParseError_En(json.GetParseError());
        SARUS_THROW_ERROR(message.str());
    }
    return json;
}

rapidjson::SchemaDocument readSchema(const boost::filesystem::path& schemaFile) {
    class RemoteSchemaDocumentProvider : public rapidjson::IRemoteSchemaDocumentProvider {
    public:
        RemoteSchemaDocumentProvider(const boost::filesystem::path& schemasDir)
            : schemasDir{schemasDir}
        {}
        const rapidjson::SchemaDocument* GetRemoteDocument(const char* uri, rapidjson::SizeType length) override {
            auto filename = std::string(uri, length);
            auto schema = json::read(schemasDir / filename);
            return new rapidjson::SchemaDocument(schema);
        }
    private:
        boost::filesystem::path schemasDir;
    };

    auto schemaJSON = json::read(schemaFile);
    auto provider = RemoteSchemaDocumentProvider{ schemaFile.parent_path() };
    return rapidjson::SchemaDocument{ schemaJSON, nullptr, rapidjson::SizeType(0), &provider };
}

rapidjson::Document readAndValidate(const boost::filesystem::path& jsonFile, const boost::filesystem::path& schemaFile) {
    auto schema = readSchema(schemaFile);

    rapidjson::Document json;

    // Use a reader object to parse the JSON storing configuration settings
    try {
        std::ifstream configInputStream(jsonFile.string());
        rapidjson::IStreamWrapper configStreamWrapper(configInputStream);
        // Parse JSON from reader, validate the SAX events, and populate the configuration Document.
        rapidjson::SchemaValidatingReader<rapidjson::kParseDefaultFlags, rapidjson::IStreamWrapper, rapidjson::UTF8<> > reader(configStreamWrapper, schema);
        json.Populate(reader);

        // Check parsing outcome
        if (!reader.GetParseResult()) {
            // Not a valid JSON
            // When reader.GetParseResult().Code() == kParseErrorTermination,
            // it may be terminated by:
            // (1) the validator found that the JSON is invalid according to schema; or
            // (2) the input stream has I/O error.
            // Check the validation result
            if (!reader.IsValid()) {
                // Input JSON is invalid according to the schema
                // Output diagnostic information
                rapidjson::StringBuffer sb;
                reader.GetInvalidSchemaPointer().StringifyUriFragment(sb);
                auto message = boost::format("Invalid schema: %s\n") % sb.GetString();
                message = boost::format("%sInvalid keyword: %s\n") % message % reader.GetInvalidSchemaKeyword();
                sb.Clear();
                reader.GetInvalidDocumentPointer().StringifyUriFragment(sb);
                message = boost::format("%sInvalid document: %s\n") % message % sb.GetString();
                // Detailed violation report is available as a JSON value
                sb.Clear();
                rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);
                reader.GetError().Accept(w);
                message = boost::format("%sError report:\n%s") % message % sb.GetString();
                SARUS_THROW_ERROR(message.str());
            }
            else {
                auto message = boost::format("Error parsing JSON file: %s") % jsonFile;
                SARUS_THROW_ERROR(message.str());
            }
        }
    }
    catch(const libsarus::Error&) {
        throw;
    }
    catch (const std::exception& e) {
        auto message = boost::format("Error reading JSON file %s") % jsonFile;
        SARUS_RETHROW_ERROR(e, message.str());
    }

    return json;
}

void write(const rapidjson::Value& json, const boost::filesystem::path& filename) {
    try {
        filesystem::createFoldersIfNecessary(filename.parent_path());
        std::ofstream ofs(filename.string());
        if(!ofs) {
            auto message = boost::format("Failed to open std::ofstream for %s") % filename;
            SARUS_THROW_ERROR(message.str());
        }
        rapidjson::OStreamWrapper osw(ofs);
        rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
        writer.SetIndent(' ', 3);
        json.Accept(writer);
    }
    catch(const std::exception& e) {
        auto message = boost::format("Failed to write JSON to %s") % filename;
        SARUS_RETHROW_ERROR(e, message.str());
    }
}

std::string serialize(const rapidjson::Value& json) {
    namespace rj = rapidjson;
    rj::StringBuffer buffer;
    rj::Writer<rj::StringBuffer> writer(buffer);
    json.Accept(writer);
    return buffer.GetString();
}

}}
