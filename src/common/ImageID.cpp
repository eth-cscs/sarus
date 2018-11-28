#include "ImageID.hpp"

#include <boost/format.hpp>


namespace sarus {
namespace common {

bool operator==(const ImageID& lhs, const ImageID& rhs) {
    return lhs.server == rhs.server
        && lhs.repositoryNamespace == rhs.repositoryNamespace
        && lhs.image == rhs.image
        && lhs.tag == rhs.tag;
}

std::ostream& operator<<(std::ostream& os, const ImageID& imageID) {
    os << imageID.getUniqueKey();
    return os;
}

std::string ImageID::getUniqueKey() const {
    auto format =  boost::format("%s/%s/%s/%s");
    return (format
            % server
            % repositoryNamespace
            % image
            % tag).str();
}

}
}
