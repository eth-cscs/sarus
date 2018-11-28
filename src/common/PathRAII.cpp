#include "PathRAII.hpp"
#include "Error.hpp"


namespace sarus {
namespace common {

PathRAII::PathRAII(const boost::filesystem::path& path)
    : path{path}
{
    try {
        this->path = path;
    }
    catch(const std::exception& e) {
        boost::filesystem::remove_all(path);
        SARUS_RETHROW_ERROR(e, "Failed to initialize PathRAII's path member");
    }
}

PathRAII::PathRAII(PathRAII&& rhs)
    : path{std::move(rhs.path)}
{}

PathRAII& PathRAII::operator=(PathRAII&& rhs) {
    path = std::move(rhs.path);
    return *this;
}

PathRAII::~PathRAII() {
    if(path) {
        boost::filesystem::remove_all(*path);
    }
}

const boost::filesystem::path& PathRAII::getPath() const {
    return *path;
}

void PathRAII::release() {
    path.reset();
}

}
}
