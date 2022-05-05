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
        CHECK(boost::filesystem::is_symlink(ociImagePath / "blobs"));
        CHECK(boost::filesystem::read_symlink(ociImagePath / "blobs") == config->directories.cache / "blobs");
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
        CHECK(boost::filesystem::is_directory(ociImagePath / "blobs"));
        boost::filesystem::remove_all(ociImagePath);
    }
}

TEST(SkopeoDriverTestGroup, inspectRaw) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    auto driver = image_manager::SkopeoDriver{config};

    auto imageReference = std::string{"quay.io/ethcscs/alpine:3.14"};
    auto expectedManifest = common::readJSON(boost::filesystem::path{__FILE__}.parent_path() / "expected_inspect_raw_manifest.json");

    auto returnedManifest = driver.inspectRaw("docker", imageReference);
    CHECK(returnedManifest == expectedManifest);

    // Check debug mode does not alter result
    common::Logger::getInstance().setLevel(common::LogLevel::DEBUG);
    returnedManifest = driver.inspectRaw("docker", imageReference);
    CHECK(returnedManifest == expectedManifest);
    common::Logger::getInstance().setLevel(common::LogLevel::WARN);
}

TEST(SkopeoDriverTestGroup, manifestDigest) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    boost::filesystem::remove_all(config->directories.cache);

    auto driver = image_manager::SkopeoDriver{config};

    auto rawManifestPath = boost::filesystem::path{__FILE__}.parent_path() / "expected_inspect_raw_manifest.json";
    CHECK(driver.manifestDigest(rawManifestPath) == std::string{"sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d"});

    // OCI image blobs have their own digests as filenames, so it's a useful property for more test cases
    auto blobsPath = boost::filesystem::path{__FILE__}.parent_path() / "saved_image_oci/blobs/sha256";
    auto testFile = blobsPath / "a64cda09ceb8b10ba4116e5b8f5628bfb72e35d7fbae76369bec728cbd839fd9";
    CHECK(driver.manifestDigest(testFile) == std::string{"sha256:a64cda09ceb8b10ba4116e5b8f5628bfb72e35d7fbae76369bec728cbd839fd9"});

    testFile = blobsPath / "2c2372178e530e6207e05f0756bb4b3018a92f62616c4af5fd4c42eb361e6079";
    CHECK(driver.manifestDigest(testFile) == std::string{"sha256:2c2372178e530e6207e05f0756bb4b3018a92f62616c4af5fd4c42eb361e6079"});

    // Test file writing by RapidJSON digests to the same value
    auto jsonManifest = common::readJSON(rawManifestPath);
    auto writtenManifest = common::makeUniquePathWithRandomSuffix(config->directories.repository / "testManifest");
    common::writeJSON(jsonManifest, writtenManifest);
    CHECK(driver.manifestDigest(writtenManifest) == std::string{"sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d"});

    // Check debug mode does not alter result
    common::Logger::getInstance().setLevel(common::LogLevel::DEBUG);
    CHECK(driver.manifestDigest(rawManifestPath) == std::string{"sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d"});
    common::Logger::getInstance().setLevel(common::LogLevel::WARN);
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
    logger.setLevel(common::LogLevel::WARN);
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
