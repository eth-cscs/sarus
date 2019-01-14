#include <memory>

#include <boost/filesystem.hpp>

#include "test_utility/config.hpp"
#include "common/Utility.hpp"
#include "image_manager/ImageStore.hpp" 
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(ImageStoreTestGroup) {
};

TEST(ImageStoreTestGroup, test_ImageStore) {
    auto config = std::make_shared<common::Config>(test_utility::config::makeConfig());
    config->imageID = {"index.docker.io", "library", "hello-world", "latest"};

    std::string dummyDigest = "XXXdigestXXX";
    time_t currentTime = time_t(nullptr);

    auto image = common::SarusImage{
        config->imageID,
        dummyDigest,
        common::SarusImage::createSizeString(size_t(1024)),
        common::SarusImage::createTimeString(currentTime),
        config->getImageFile(),
        config->getMetadataFileOfImage()};

    auto imageStore = image_manager::ImageStore{ config };

    // clean repository
    boost::filesystem::remove_all(config->directories.repository);
    common::createFoldersIfNecessary(config->directories.repository);
    CHECK(imageStore.listImages().empty());

    // add image
    imageStore.addImage(image);
    CHECK(boost::filesystem::exists(imageStore.getMetadataFile()));
    auto expectedImages = std::vector<common::SarusImage>{ image };
    CHECK(imageStore.listImages() == expectedImages);

    // add same image another time
    imageStore.addImage(image);
    CHECK(boost::filesystem::exists(imageStore.getMetadataFile()));
    CHECK(imageStore.listImages() == expectedImages);

    // remove image
    imageStore.removeImage(config->imageID);
    CHECK(imageStore.listImages().empty());

    // cleanup
    boost::filesystem::remove_all(config->directories.repository);
}

SARUS_UNITTEST_MAIN_FUNCTION();
