/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/Logger.hpp"
#include "common/Utility.hpp"
#include "image_manager/SkopeoDriver.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace image_manager {
namespace test {

TEST_GROUP(SkopeoDriverTestGroup) {
};

TEST(SkopeoDriverTestGroup, copyToOCIImage) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    boost::filesystem::remove_all(config->directories.cache);

    auto driver = image_manager::SkopeoDriver{config};

    // Docker registry
    {
        auto imageReference = std::string{"quay.io/ethcscs/alpine:3.14"};
        auto ociImagePath = driver.copyToOCIImage("docker", imageReference);
        auto imageIndex = common::readJSON(ociImagePath / "index.json");
        std::string imageRef;
        try {
            imageRef = imageIndex["manifests"][0]["annotations"]["org.opencontainers.image.ref.name"].GetString();
        }
        catch (std::exception& e) {
            FAIL("Could not find OCI Image ref name inside index.json");
        }
        CHECK_EQUAL(imageRef, std::string{"sarus-oci-image"});
        boost::filesystem::remove_all(ociImagePath);
    }
    // Docker archive
    {
        auto archive = boost::filesystem::path{__FILE__}.parent_path() / "saved_image.tar";
        auto ociImagePath = driver.copyToOCIImage("docker-archive", archive.string());
        auto imageIndex = common::readJSON(ociImagePath / "index.json");
        std::string imageRef;
        try {
            imageRef = imageIndex["manifests"][0]["annotations"]["org.opencontainers.image.ref.name"].GetString();
        }
        catch (std::exception& e) {
            FAIL("Could not find OCI Image ref name inside index.json");
        }
        CHECK_EQUAL(imageRef, std::string{"sarus-oci-image"});
        boost::filesystem::remove_all(ociImagePath);
    }
}

TEST(SkopeoDriverTestGroup, inspect) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    boost::filesystem::remove_all(config->directories.cache);

    auto driver = image_manager::SkopeoDriver{config};

    auto imageReference = std::string{"quay.io/ethcscs/alpine:3.14"};
    auto returnedManifest = driver.inspect("docker", imageReference);

    auto expectedManifest = common::readJSON(boost::filesystem::path{__FILE__}.parent_path() / "expected_inspect_manifest.json");
    CHECK(returnedManifest == expectedManifest);
}

TEST(SkopeoDriverTestGroup, generateBaseArgs) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    auto driver = image_manager::SkopeoDriver{config};
    auto& logger = common::Logger::getInstance();

    logger.setLevel(common::LogLevel::DEBUG);
    auto skopeoArgs = driver.generateBaseArgs();
    auto expectedArgs = common::CLIArguments{"/usr/bin/skopeo", "--debug"};
    CHECK(skopeoArgs == expectedArgs);

    logger.setLevel(common::LogLevel::INFO);
    skopeoArgs = driver.generateBaseArgs();
    expectedArgs = common::CLIArguments{"/usr/bin/skopeo"};
    CHECK(skopeoArgs == expectedArgs);

    logger.setLevel(common::LogLevel::WARN);
    skopeoArgs = driver.generateBaseArgs();
    expectedArgs = common::CLIArguments{"/usr/bin/skopeo"};
    CHECK(skopeoArgs == expectedArgs);

    logger.setLevel(common::LogLevel::ERROR);
    skopeoArgs = driver.generateBaseArgs();
    expectedArgs = common::CLIArguments{"/usr/bin/skopeo"};
    CHECK(skopeoArgs == expectedArgs);
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
