#include "image_manager/Puller.hpp"
#include "image_manager/PulledImage.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace image_manager {
namespace test {

TEST_GROUP(PulledImageTestGroup) {
};

TEST(PulledImageTestGroup, test) {
    auto config = test_utility::config::makeConfig();
    config.imageID = {"index.docker.io", "library", "alpine", "3.8"};

    // pull
    auto puller = image_manager::Puller{config};
    auto manifest = puller.getManifest();
    puller.pull();

    // expand
    auto pulledImage = PulledImage{config, manifest};
    common::PathRAII expandedImage;
    common::ImageMetadata metadata;
    std::tie(expandedImage, metadata, std::ignore) = pulledImage.expand();

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
