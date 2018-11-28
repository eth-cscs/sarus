#include "image_manager/LoadedImage.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace image_manager {
namespace test {

TEST_GROUP(LoadedImageTestGroup) {
};

TEST(LoadedImageTestGroup, test) {
    auto config = test_utility::config::makeConfig();

    // expand
    auto archive = boost::filesystem::path{__FILE__}.parent_path() / "saved_image.tar";
    auto loadedImage = LoadedImage(config, archive);
    common::PathRAII expandedImage;
    common::ImageMetadata metadata;
    std::tie(expandedImage, metadata, std::ignore) = loadedImage.expand();

    // check
    auto expectedDirectory = expandedImage.getPath() / "etc";
    CHECK(boost::filesystem::exists(expectedDirectory));

    auto expectedFile = expandedImage.getPath() / "etc/os-release";
    CHECK(boost::filesystem::exists(expectedFile));

    auto expectedMetadata = common::ImageMetadata{};
    expectedMetadata.cmd = common::CLIArguments{"/bin/sh"};
    expectedMetadata.env["PATH"] = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    CHECK(metadata == expectedMetadata);

    // cleanup
    boost::filesystem::remove_all(config.directories.repository);
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
