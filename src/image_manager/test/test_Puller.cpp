/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
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

SARUS_UNITTEST_MAIN_FUNCTION();
