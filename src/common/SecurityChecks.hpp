#ifndef sarus_runtime_SecurityChecks_hpp
#define sarus_runtime_SecurityChecks_hpp

#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "common/Config.hpp"


namespace sarus {
namespace common {

class SecurityChecks {
public:
    void checkThatFileIsUntamperable(const boost::filesystem::path&) const;
    void checkThatBinariesInSarusJsonAreUntamperable(const rapidjson::Document&) const;
    void checkThatOCIHooksAreUntamperable(const common::Config&) const;

private:
    void checkThatOCIHooksAreUntamperableByType(const common::Config&, const std::string& hookType) const;
};

}
}

#endif
