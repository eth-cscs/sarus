#ifndef sarus_image_manger_LoadedImage_hpp
#define sarus_image_manger_LoadedImage_hpp

#include <boost/filesystem.hpp>

#include "InputImage.hpp"


namespace sarus {
namespace image_manager {

/**
 * This class represents an image to be loaded that has not been expanded yet.
 */
class LoadedImage : public InputImage {
public:
    LoadedImage(const common::Config& config, const boost::filesystem::path& imageArchive);
    std::tuple<common::PathRAII, common::ImageMetadata, std::string> expand() const override;

private:
    boost::filesystem::path imageArchive;
};

}
}

#endif
