#ifndef sarus_common_ImageMetadata_hpp
#define sarus_common_ImageMetadata_hpp

#include <string>
#include <unordered_map>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "common/CLIArguments.hpp"

namespace sarus {
namespace common {

class ImageMetadata {
public:
    ImageMetadata() = default;
    ImageMetadata(const boost::filesystem::path& path);
    ImageMetadata(const rapidjson::Value& metadata);
    boost::optional<common::CLIArguments> cmd;
    boost::optional<common::CLIArguments> entry;
    boost::optional<boost::filesystem::path> workdir;
    std::unordered_map<std::string, std::string> env;
    void write(const boost::filesystem::path& path) const;

private:
    void parseJSON(const rapidjson::Value& json);
};

bool operator==(const ImageMetadata&, const ImageMetadata&);

}
}

#endif
