/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <boost/filesystem.hpp>

#include "test_utility/config.hpp"
#include "image_manager/Puller.hpp" 
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(PullerTestGroup) {
    
};

TEST(PullerTestGroup, testGetManifest) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    config->imageID = common::ImageID{"quay.io", "ethcscs", "alpine", "3.14"};

    auto puller = image_manager::Puller{config};

    boost::filesystem::remove_all(config->directories.cache);

    // test manifest
    web::json::value manifest = puller.retrieveImageManifest();
    CHECK( !manifest.has_field(U("errors")) );
    CHECK( manifest.at(U("name")) == web::json::value(config->imageID.repositoryNamespace + "/" + config->imageID.image) );
    CHECK( manifest.at(U("tag"))  == web::json::value(config->imageID.tag) );

    puller.pull();
}

TEST(PullerTestGroup, testGetProxy) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    config->imageID = common::ImageID{"index.docker.io", "ethcscs", "alpine", "latest"};

    auto puller = image_manager::Puller{config};

    boost::filesystem::remove_all(config->directories.cache);

    // Ensure environment is clean
    if(clearenv() != 0) {
        FAIL("Failed to clear host environment variables");
    }

    // Test no variable set
    CHECK(puller.getProxy().empty());

    // Test http_proxy
    config->enforceSecureServer = false;
    config->commandRun.hostEnvironment["http_proxy"] = std::string("http://proxy.test.com:3128");
    CHECK_EQUAL(puller.getProxy(), "http://proxy.test.com:3128");

    // Test HTTPS_PROXY
    // Notice that from this point on we don't remove env vars set for previous cases,
    // so we check that variable priority is actually enforced
    config->enforceSecureServer = true;
    config->commandRun.hostEnvironment["HTTPS_PROXY"] = std::string("https://uppercase.proxy.com");
    CHECK_EQUAL(puller.getProxy(), "https://uppercase.proxy.com");

    // Test https_proxy
    config->commandRun.hostEnvironment["https_proxy"] = std::string("https://lowercase.proxy.com");
    CHECK_EQUAL(puller.getProxy(), "https://lowercase.proxy.com");

    // Test ALL_PROXY
    config->commandRun.hostEnvironment["ALL_PROXY"] = std::string("https://all.proxy.com");
    CHECK_EQUAL(puller.getProxy(), "https://all.proxy.com");

    // Test NO_PROXY
    {
        config->commandRun.hostEnvironment["NO_PROXY"] = std::string("test.domain.com");
        CHECK_EQUAL(puller.getProxy(), "https://all.proxy.com");

        config->commandRun.hostEnvironment["NO_PROXY"] = std::string("test.domain.com,index.docker.io");
        CHECK(puller.getProxy().empty());

        config->commandRun.hostEnvironment["NO_PROXY"] = std::string("*");
        CHECK(puller.getProxy().empty());
    }

    // Test no_proxy
    {
        config->commandRun.hostEnvironment["no_proxy"] = std::string("test.domain.com");
        CHECK_EQUAL(puller.getProxy(), "https://all.proxy.com");

        // Reset upper case variable to keep it different from lower case
        config->commandRun.hostEnvironment["NO_PROXY"] = std::string("test.domain.com");
        config->commandRun.hostEnvironment["no_proxy"] = std::string("test.domain.com,index.docker.io");
        CHECK(puller.getProxy().empty());

        config->commandRun.hostEnvironment["no_proxy"] = std::string("*");
        CHECK(puller.getProxy().empty());
    }
}

SARUS_UNITTEST_MAIN_FUNCTION();
