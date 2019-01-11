#ifndef sarus_runtime_SecurityChecks_hpp
#define sarus_runtime_SecurityChecks_hpp

#include <memory>

#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "common/Config.hpp"


namespace sarus {
namespace common {

class SecurityChecks {
public:
    SecurityChecks(std::shared_ptr<const common::Config>);
    void checkThatPathIsUntamperable(const boost::filesystem::path&) const;
    void checkThatBinariesInSarusJsonAreUntamperable(const rapidjson::Document&) const;
    void checkThatOCIHooksAreUntamperable() const;

private:
    void checkThatOCIHooksAreUntamperableByType(const std::string& hookType) const;

private:
    std::shared_ptr<const common::Config> config;
};

}
}

#endif
