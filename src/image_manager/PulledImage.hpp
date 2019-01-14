#ifndef sarus_image_manger_PulledImage_hpp
#define sarus_image_manger_PulledImage_hpp

#include <vector>
#include <memory>
#include <boost/filesystem.hpp>
#include <cpprest/json.h>

#include "InputImage.hpp"


namespace sarus {
namespace image_manager {

/**
 * This class represents a pulled image that has not been expanded yet.
 */
class PulledImage : public InputImage {
public:
    PulledImage(std::shared_ptr<const common::Config> config, web::json::value& manifest);
    std::tuple<common::PathRAII, common::ImageMetadata, std::string> expand() const override;

private:
    void initializeListOfLayersAndMetadata(web::json::value &manifest);

private:
    std::vector<boost::filesystem::path> layers;
    common::ImageMetadata metadata;
    std::string digest;
};

}
}

#endif
