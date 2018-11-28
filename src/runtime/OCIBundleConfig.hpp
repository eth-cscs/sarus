#ifndef sarus_runtime_OCIBundleConfig_hpp
#define sarus_runtime_OCIBundleConfig_hpp

#include <memory>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "common/Config.hpp"
#include "runtime/ConfigsMerger.hpp"


namespace sarus {
namespace runtime {

class OCIBundleConfig {
public:
    OCIBundleConfig(const common::Config&);
    void generateConfigFile() const;
    const boost::filesystem::path& getConfigFile() const;

private:
    void makeJsonDocument() const;
    rapidjson::Value makeMemberProcess() const;
    rapidjson::Value makeMemberRoot() const;
    rapidjson::Value makeMemberMounts() const;
    rapidjson::Value makeMemberLinux() const;
    rapidjson::Value makeMemberHooks() const;

private:
    const common::Config* config;
    ConfigsMerger configsMerger;
    std::shared_ptr<rapidjson::Document> document;
    rapidjson::MemoryPoolAllocator<>* allocator;
    boost::filesystem::path configFile;
};

}
}

#endif
