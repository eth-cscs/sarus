/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "image_manager/Utility.hpp"

#include <sstream>

#include <boost/predef.h>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>

#include "common/Error.hpp"
#include "common/Utility.hpp"


namespace rj = rapidjson;

namespace sarus {
namespace image_manager {
namespace utility {

/**
 * Return data about current platform in the JSON format defined by the OCI Image spec
 * More details at https://github.com/opencontainers/image-spec/blob/v1.0.2/image-index.md#image-index-property-descriptions
 */
rapidjson::Document getCurrentOCIPlatform() {
    auto architecture = std::string{};
    auto variant = std::string{};

    #if BOOST_ARCH_X86
        #if BOOST_ARCH_X86_64
            architecture = "amd64";
        #elif BOOST_ARCH_X86_32
            architecture = "386";
        #endif

    #elif BOOST_ARCH_ARM
        #if BOOST_ARCH_WORD_BITS_64
            architecture = "arm64";
        #elif BOOST_ARCH_WORD_BITS_32
            architecture = "arm";
        #endif
            auto variant = std::string{"v"} + BOOST_VERSION_NUMBER_MAJOR(BOOST_ARCH_ARM);

    #elif BOOST_ARCH_PPC64
        #if BOOST_ENDIAN_LITTLE_BYTE || BOOST_ENDIAN_LITTLE_WORD
            architecture = "ppc64le";
        #else
            architecture = "ppc64";
        #endif

    #elif BOOST_ARCH_MIPS
        #if BOOST_ARCH_WORD_BITS_64
            #if BOOST_ENDIAN_LITTLE_BYTE || BOOST_ENDIAN_LITTLE_WORD
                architecture = "mips64le";
            #else
                architecture = "mips64";
            #endif
        #elif BOOST_ARCH_WORD_BITS_32
            #if BOOST_ENDIAN_LITTLE_BYTE || BOOST_ENDIAN_LITTLE_WORD
                architecture = "mipsle";
            #else
                architecture = "mips";
            #endif
        #endif

    #elif BOOST_ARCH_RISCV
        architecture = "riscv64";

    #elif BOOST_ARCH_SYS390
        architecture = "s390x";

    #else
        #error "Failed to detect CPU architecture for determining current OCI platform"
    #endif

    if (architecture.empty()) {
        SARUS_THROW_ERROR("Failed to detect CPU architecture ");
    }

    auto platform = rj::Document{rj::kObjectType};
    auto allocator = platform.GetAllocator();
    platform.AddMember("os", rj::Value{"linux"}, allocator);
    platform.AddMember("architecture", rj::Value{architecture.c_str(), allocator}, allocator);
    platform.AddMember("variant", rj::Value{variant.c_str(), allocator}, allocator);

    auto message = boost::format("Detected current platform: %s") % common::serializeJSON(platform);
    printLog(message, common::LogLevel::DEBUG);

    return platform;
}

std::string getPlatformDigestFromOCIIndex(const rj::Document& index, const rj::Document& targetPlatform) {
    auto output = std::string{};

    for (const auto& manifestProperties : index["manifests"].GetArray()) {
        // According to the OCI Image spec, platform data is optional, but we
        // need it here to look for a specific manifest
        auto platformItr =  manifestProperties.FindMember("platform");
        if (platformItr == manifestProperties.MemberEnd()) {
            SARUS_THROW_ERROR("Failed to find 'platform' property for manifest in image index");
        }

        auto platform = platformItr->value.GetObject();
        if (platform["os"] != targetPlatform["os"]) {
            continue;
        }
        if (platform["architecture"] != targetPlatform["architecture"]) {
            continue;
        }
        auto variantItr = platform.FindMember("variant");
        if (variantItr == platform.MemberEnd()) {
            // OS and CPU arch match, but there is no data on CPU variant
            // Store this digest as the best match so far
            output = manifestProperties["digest"].GetString();
        }
        else if (variantItr->value == targetPlatform["variant"]) {
            // Exact match on CPU variant too: get digest and quit loop
            output = manifestProperties["digest"].GetString();
            break;
        }
    }

    if (output.empty()) {
        printLog("Failed to find manifest matching current platform in image index", common::LogLevel::WARN);
    }
    else {
        auto message = boost::format("Found manifest digest in OCI index: %s") % output;
        printLog(message, common::LogLevel::DEBUG);
    }
    return output;
}

std::string base64Encode(const std::string& input) {
    namespace bai = boost::archive::iterators;
    typedef std::string::const_iterator iterator_type;

    // Retrieve 6 bit integers from a sequence of 8 bit bytes
    // and convert binary values to base64 characters
    typedef bai::base64_from_binary<bai::transform_width<iterator_type, 6, 8>> base64_enc;

    std::stringstream ss;
    std::copy(base64_enc(input.begin()), base64_enc(input.end()), std::ostream_iterator<char>(ss));

    // Pad with '=' if the input string is not a multiple of 3
    const std::string base64Padding[] = {"", "==","="};
    ss << base64Padding[input.size() % 3];

    return ss.str();
}

void printLog(const boost::format& message, common::LogLevel level,
              std::ostream& outStream, std::ostream& errStream) {
    printLog(message.str(), level, outStream, errStream);
}

void printLog(const std::string& message, common::LogLevel level,
              std::ostream& outStream, std::ostream& errStream) {
    common::Logger::getInstance().log(message, "ImageManager_Utility", level, outStream, errStream);
}

} // namespace
} // namespace
} // namespace
