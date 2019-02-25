#include <memory>

#include "image_manager/LoadedImage.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace image_manager {
namespace test {

TEST_GROUP(InputImageTestGroup) {
};

TEST(InputImageTestGroup, image_with_nonexecutable_directory) {
    auto config = std::make_shared<common::Config>(test_utility::config::makeConfig());
    auto archive = boost::filesystem::path{__FILE__}.parent_path() / "saved_image_with_non-executable_dir.tar";
    auto loadedImage = LoadedImage(config, archive);
    loadedImage.expand();
}

TEST(InputImageTestGroup, image_with_whiteouts) {
    auto config = std::make_shared<common::Config>(test_utility::config::makeConfig());
    auto archive = boost::filesystem::path{__FILE__}.parent_path() / "saved_image_with_whiteouts.tar";
    auto loadedImage = LoadedImage(config, archive);
    common::PathRAII expandedImage;
    std::tie(expandedImage, std::ignore, std::ignore) = loadedImage.expand();
    CHECK(boost::filesystem::is_empty(expandedImage.getPath() / "dir-with-whiteout"));
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
