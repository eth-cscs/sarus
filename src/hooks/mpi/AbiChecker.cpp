#if __cplusplus < 201300
    #include <memory>
#endif
#include<utility>

#include "AbiChecker.hpp"

#include "libsarus/Error.hpp"
#include "libsarus/Utility.hpp"

namespace sarus {
namespace hooks {
namespace mpi {

// Expected behaviour:
// if the major ABI number matches or is higher returns boost::none
// else throws
std::pair<bool, boost::optional<boost::format>> MajorAbiCompatibilityChecker::check(const SharedLibrary &hostLib, const SharedLibrary &containerLib) const {
    if (areMajorAbiCompatible(hostLib, containerLib)) {
        return std::make_pair(true, boost::none);
    }
    auto message = boost::format(
                       "Failed to activate MPI support. Host's MPI library %s is not ABI"
                       " compatible with container's MPI library %s.") %
                   hostLib.getRealName() % containerLib.getRealName();
    SARUS_THROW_ERROR(message.str());
}

// Expected behaviour:
// if the (major, minor) ABI numbers match returns boost::none
// if the major ABI number matches returns a message
// else throws
std::pair<bool, boost::optional<boost::format>> FullAbiCompatibilityChecker::check(const SharedLibrary &hostLib, const SharedLibrary &containerLib) const {
    if (areFullAbiCompatible(hostLib, containerLib)) {
        return std::make_pair(true, boost::none);
    }
    if (areMajorAbiCompatible(hostLib, containerLib)) {
        return std::make_pair(true, boost::format("Partial ABI compatibility detected. Host's MPI library %s is older than"
                             " the container's MPI library %s. The hook will attempt to proceed with the library replacement."
                             " Be aware that applications are likely to fail if they use symbols which are only present in the container's library."
                             " More information available at https://sarus.readthedocs.io/en/stable/user/abi_compatibility.html") %
               hostLib.getRealName() % containerLib.getRealName());
    }
    auto message = boost::format(
                       "Failed to activate MPI support. Host's MPI library %s is not ABI"
                       " compatible with container's MPI library %s.") %
                   hostLib.getRealName() % containerLib.getRealName();
    SARUS_THROW_ERROR(message.str());
}

std::pair<bool, boost::optional<boost::format>> StrictAbiCompatibilityChecker::check(const SharedLibrary &hostLib, const SharedLibrary &containerLib) const {
    if (areStrictlyAbiCompatible(hostLib, containerLib)) {
        return std::make_pair(true, boost::none);
    }
    auto message = boost::format(
                       "Failed to activate MPI support. Host's MPI library %s is not strictly ABI"
                       " compatible with container's MPI library %s.") %
                   hostLib.getRealName() % containerLib.getRealName();
    SARUS_THROW_ERROR(message.str());
}

std::pair<bool, boost::optional<boost::format>> DependenciesAbiCompatibilityChecker::check(const SharedLibrary &hostLib, const SharedLibrary &containerLib) const {
    try {
        return FullAbiCompatibilityChecker{}.check(hostLib, containerLib);
    } catch (const libsarus::Error&) {}
    auto message = boost::format{"Could not find ABI-compatible counterpart for host lib (%s) inside container (best candidate found: %s) => adding host lib (%s) into container's /lib via bind mount "} % hostLib.getPath() % containerLib.getPath() % hostLib.getPath();
    //auto message = boost::format{"WARNING: container lib (%s) is major-only-abi-compatible => bind mount host lib (%s) into /lib"} % containerLib.getPath() % hostLib.getPath();
    return std::make_pair(false, message);
}

#if __cplusplus < 201300
template<class T, class... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#else
using std::make_unique;
#endif

AbiCheckerFactory::AbiCheckerFactory() {
    abiCompatibilityCheckerMap[std::string{"major"}] = [] { return make_unique<MajorAbiCompatibilityChecker>(); };
    abiCompatibilityCheckerMap[std::string{"full"}] = [] { return make_unique<FullAbiCompatibilityChecker>(); };
    abiCompatibilityCheckerMap[std::string{"strict"}] = [] { return make_unique<StrictAbiCompatibilityChecker>(); };
    abiCompatibilityCheckerMap[std::string{"dependencies"}] = [] { return make_unique<DependenciesAbiCompatibilityChecker>(); };

    for (auto checkerItem{abiCompatibilityCheckerMap.begin()}; checkerItem!=abiCompatibilityCheckerMap.end(); ++checkerItem) { 
        checkerTypes.insert(checkerItem->first);
    } 
}

  std::unique_ptr<AbiCompatibilityChecker> AbiCheckerFactory::create(const std::string& Type) { return abiCompatibilityCheckerMap[Type](); }


}}}
