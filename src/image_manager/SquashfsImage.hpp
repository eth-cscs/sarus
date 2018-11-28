#ifndef sarus_image_manger_SquashfsImage_hpp
#define sarus_image_manger_SquashfsImage_hpp

#include <boost/filesystem.hpp>

#include "common/Config.hpp"


namespace sarus {
namespace image_manager {

/**
 * This class builds and represents the squashfs image.
 */
class SquashfsImage {
public:
    SquashfsImage(  const common::Config& config,
                    const boost::filesystem::path& expandedImage,
                    const boost::filesystem::path& pathOfImage);
    boost::filesystem::path getPathOfImage() const;

private:
    void log(const boost::format &message, common::logType level) const;

private:
    boost::filesystem::path pathOfImage;
};

}
}

#endif
