#include <boost/filesystem.hpp>

#include "test_utility/config.hpp"
#include "image_manager/Puller.hpp" 
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(PullerTestGroup) {
    
};

TEST(PullerTestGroup, testGetManifest) {
    auto config = test_utility::config::makeConfig();
    config.imageID = common::ImageID{"index.docker.io", "library", "hello-world", "latest"};

    auto puller = image_manager::Puller{config};

    boost::filesystem::remove_all(config.directories.cache);

    // test manifest
    web::json::value manifest = puller.getManifest();
    CHECK( !manifest.has_field(U("errors")) );
    CHECK( manifest.at(U("name")) == web::json::value(config.imageID.repositoryNamespace + "/" + config.imageID.image) );
    CHECK( manifest.at(U("tag"))  == web::json::value(config.imageID.tag) );

    puller.pull();

    // cleanup
    boost::filesystem::remove_all(config.directories.repository);
}

SARUS_UNITTEST_MAIN_FUNCTION();
