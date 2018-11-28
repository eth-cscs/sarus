#ifndef sarus_image_manger_InputImage_hpp
#define sarus_image_manger_InputImage_hpp

#include <tuple>
#include <vector>
#include <archive.h> // libarchive
#include <boost/filesystem.hpp>

#include "common/Config.hpp"
#include "common/PathRAII.hpp"
#include "common/ImageMetadata.hpp"

namespace sarus {
namespace image_manager {

/**
 * This class represents an input image prior extraction.
 */
class InputImage {
public:
    InputImage(const common::Config& config);
    virtual std::tuple<common::PathRAII, common::ImageMetadata, std::string> expand() const = 0;

protected:
    boost::filesystem::path makeTemporaryExpansionDirectory() const;
    void expandLayers(  const std::vector<boost::filesystem::path>& layersPaths,
                        const boost::filesystem::path& expandDir) const;
    void extractArchive(const boost::filesystem::path& archivePath) const;
    void extractArchiveWithExcludePatterns( const boost::filesystem::path& archivePath,
                                            const std::vector<std::string> &excludePattern,
                                            std::vector<std::string> &whiteouts) const;
    void copyDataOfArchiveEntry(const boost::filesystem::path& archivePath,
                                ::archive* in,
                                ::archive* out,
                                ::archive_entry *entry) const;
    void log(const boost::format &message, common::logType level) const;
    void log(const std::string& message, common::logType level) const;

protected:
    const common::Config* config;
};

}
}

#endif
