/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <rapidjson/document.h>

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
    auto expectedManifest = common::readFile(boost::filesystem::path{__FILE__}.parent_path() / "expected_inspect_raw_manifest.json");

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

TEST(SkopeoDriverTestGroup, generateBaseArgs_verbosity) {
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

TEST(SkopeoDriverTestGroup, generateBaseArgs_policy) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    auto customPolicyPath = config->json["prefixDir"].GetString() + std::string{"/etc/policy.json"};
    common::createFileIfNecessary(customPolicyPath);
    rapidjson::Pointer("/containersPolicy/path").Set(config->json, customPolicyPath.c_str());

    auto homeMock = boost::filesystem::path{config->json["localRepositoryBaseDir"].GetString() + std::string{"/homeMock"}};
    auto userPolicyMock = homeMock / ".config/containers/policy.json";
    common::setEnvironmentVariable("HOME", homeMock.string());

    /**
     * The unit tests for `copy` and `inspect` commands already implicitly test
     * detection and usage of the system default /etc/containers/policy.json file.
     *
     * Absence of the system default policy is not testable without acquiring
     * superuser privileges and modifying the testing environment's /etc/containers
     * directory, which is considered overkill for this test program.
     *
     * The remaining cases involving user-specific policy file and Sarus configuration
     * enforcement are covered below.
     */

    // User-specific default policy file
    {
        common::createFileIfNecessary(userPolicyMock);

        image_manager::SkopeoDriver driver{config};
        auto skopeoArgs = driver.generateBaseArgs();
        auto expectedArgs = common::CLIArguments{"/usr/bin/skopeo"};
        CHECK(skopeoArgs == expectedArgs);
    }
    // Enforce custom path even with default file present
    {
        rapidjson::Pointer("/containersPolicy/enforce").Set(config->json, true);

        image_manager::SkopeoDriver driver{config};
        auto skopeoArgs = driver.generateBaseArgs();
        auto expectedPolicyValue = customPolicyPath;
        auto expectedArgs = common::CLIArguments{"/usr/bin/skopeo", "--policy", expectedPolicyValue};
        CHECK(skopeoArgs == expectedArgs);
    }
    // Custom path configured to non-existent file
    {
        boost::filesystem::remove(customPolicyPath);
        CHECK_THROWS(common::Error, image_manager::SkopeoDriver{config});
    }
}

TEST(SkopeoDriverTestGroup, generateBaseArgs_registriesd) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    auto customRegistriesDPath = config->json["prefixDir"].GetString() + std::string{"/etc/registries.d"};
    common::createFoldersIfNecessary(customRegistriesDPath);
    rapidjson::Pointer("/containersRegistries.dPath").Set(config->json, customRegistriesDPath.c_str());

    // Expected behavior
    {
        image_manager::SkopeoDriver driver{config};
        auto skopeoArgs = driver.generateBaseArgs();
        auto expectedArgs = common::CLIArguments{"/usr/bin/skopeo", "--registries.d", customRegistriesDPath};
        CHECK(skopeoArgs == expectedArgs);
    }
    // Non-existent directory
    {
        boost::filesystem::remove(customRegistriesDPath);
        CHECK_THROWS(common::Error, image_manager::SkopeoDriver{config});
    }
}

TEST(SkopeoDriverTestGroup, acquireAuthFile) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;

    config->authentication.isAuthenticationNeeded = true;
    config->authentication.username = "alice";
    config->authentication.password = "Aw3s0m&_P@s5w0rD";

    config->imageReference = common::ImageReference{"test.registry.io", "foo", "private-image", "latest", ""};

    auto xdgRuntimeDir = config->directories.repository / "xdg_runtime_dir";

    auto expectedAuthJSON = rapidjson::Document{};
    rapidjson::Pointer("/auths/test.registry.io~1foo~1private-image/auth").Set(expectedAuthJSON, "YWxpY2U6QXczczBtJl9QQHM1dzByRA==");

    auto expectedPermissions = boost::filesystem::perms::owner_read | boost::filesystem::perms::owner_write;

    // create file in XDG_RUNTIME_DIR if defined and existing
    {
        common::createFoldersIfNecessary(xdgRuntimeDir);
        common::setEnvironmentVariable("XDG_RUNTIME_DIR", xdgRuntimeDir.string());
        auto expectedAuthFilePath = xdgRuntimeDir / "sarus/auth.json";

        // Instantiate the SkopeoDriver object in another scope to check removal
        // of the authFile by the destructor
        {
            CHECK_FALSE(boost::filesystem::exists(expectedAuthFilePath));

            auto driver = image_manager::SkopeoDriver{config};
            driver.acquireAuthFile(config->authentication, config->imageReference);

            CHECK(boost::filesystem::exists(expectedAuthFilePath));
            CHECK(boost::filesystem::status(expectedAuthFilePath).permissions() == expectedPermissions);
            CHECK(common::readJSON(expectedAuthFilePath) == expectedAuthJSON);
        }
        CHECK_FALSE(boost::filesystem::exists(expectedAuthFilePath));
    }
    // create file in Sarus local repo if XDG_RUNTIME_DIR defined but not existing
    {
        boost::filesystem::remove_all(xdgRuntimeDir);
        auto expectedAuthFilePath = config->directories.repository / "auth.json";
        {
            CHECK_FALSE(boost::filesystem::exists(expectedAuthFilePath));

            auto driver = image_manager::SkopeoDriver{config};
            driver.acquireAuthFile(config->authentication, config->imageReference);

            CHECK(boost::filesystem::exists(expectedAuthFilePath));
            CHECK(boost::filesystem::status(expectedAuthFilePath).permissions() == expectedPermissions);
            CHECK(common::readJSON(expectedAuthFilePath) == expectedAuthJSON);
        }
        CHECK_FALSE(boost::filesystem::exists(expectedAuthFilePath));
    }
    // create file in Sarus local repo if XDG_RUNTIME_DIR not defined
    {
        common::createFoldersIfNecessary(xdgRuntimeDir);
        unsetenv("XDG_RUNTIME_DIR");
        auto expectedAuthFilePath = config->directories.repository / "auth.json";
        {
            CHECK_FALSE(boost::filesystem::exists(expectedAuthFilePath));

            auto driver = image_manager::SkopeoDriver{config};
            driver.acquireAuthFile(config->authentication, config->imageReference);

            CHECK(boost::filesystem::exists(expectedAuthFilePath));
            CHECK(boost::filesystem::status(expectedAuthFilePath).permissions() == expectedPermissions);
            CHECK(common::readJSON(expectedAuthFilePath) == expectedAuthJSON);
        }
        CHECK_FALSE(boost::filesystem::exists(expectedAuthFilePath));
    }
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
