#ifndef sarus_runtime_SiteMount_hpp
#define sarus_runtime_SiteMount_hpp

#include <sys/mount.h>

#include <boost/filesystem.hpp>

#include "common/Config.hpp"
#include "runtime/Mount.hpp"

namespace sarus {
namespace runtime {

class SiteMount : public Mount {
public:
    SiteMount(  const boost::filesystem::path& source,
                const boost::filesystem::path& destination,
                const unsigned long mountFlags,
                const common::Config& config);

    void performMount() const override;

public: // public for test purpose
    boost::filesystem::path source;
    boost::filesystem::path destination;
    unsigned long mountFlags;

private:
    const common::Config* config;
};

}
}

#endif
