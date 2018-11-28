#ifndef sarus_runtime_Runtime_hpp
#define sarus_runtime_Runtime_hpp

#include "common/Config.hpp"
#include "common/SecurityChecks.hpp"
#include "runtime/OCIBundleConfig.hpp"


namespace sarus {
namespace runtime {

class Runtime {
public:
    Runtime(const common::Config&);
    void setupOCIBundle() const;
    void executeContainer() const;

private:
    void setupMountIsolation() const;
    void setupRamFilesystem() const;
    void mountImageIntoRootfs() const;
    void setupDevFilesystem() const;
    void copyEtcFilesIntoRootfs() const;
    void performCustomMounts() const;
    void remountRootfsWithNoSuid() const;

private:
    const common::Config* config;
    boost::filesystem::path bundleDir;
    boost::filesystem::path rootfsDir;
    OCIBundleConfig bundleConfig;
    common::SecurityChecks securityChecks;
};

}
}

#endif
