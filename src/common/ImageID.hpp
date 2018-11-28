#ifndef sarus_common_ImageID_hpp
#define sarus_common_ImageID_hpp

#include <string>
#include <ostream>


namespace sarus {
namespace common {

struct ImageID {
    std::string server;
    std::string repositoryNamespace;
    std::string image;
    std::string tag;
    std::string getUniqueKey() const;
};

bool operator==(const ImageID&, const ImageID&);

std::ostream& operator<<(std::ostream&, const ImageID&);

}
}

#endif
