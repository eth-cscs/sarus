#ifndef sarus_hooks_mpi_AbiChecker_hpp
#define sarus_hooks_mpi_AbiChecker_hpp

#include <map>
#include <set>

#include <boost/format.hpp>
#include <boost/optional.hpp>

#include "SharedLibrary.hpp"

namespace sarus {
namespace hooks {
namespace mpi {

struct AbiCompatibilityChecker {
  virtual std::pair<bool, boost::optional<boost::format>> check(const SharedLibrary &hostLib, const SharedLibrary &containerLib) const = 0;
  virtual ~AbiCompatibilityChecker() = default;
};

struct MajorAbiCompatibilityChecker : public AbiCompatibilityChecker {
  std::pair<bool, boost::optional<boost::format>> check(const SharedLibrary &, const SharedLibrary &) const override;
};

struct FullAbiCompatibilityChecker : public AbiCompatibilityChecker {
  std::pair<bool, boost::optional<boost::format>> check(const SharedLibrary &, const SharedLibrary &) const override;
};

struct StrictAbiCompatibilityChecker : public AbiCompatibilityChecker {
  std::pair<bool, boost::optional<boost::format>> check(const SharedLibrary &, const SharedLibrary &) const override;
};

struct DependenciesAbiCompatibilityChecker : public AbiCompatibilityChecker {
  std::pair<bool, boost::optional<boost::format>> check(const SharedLibrary &, const SharedLibrary &) const override;
};

class AbiCheckerFactory {
  std::map<std::string, std::function<std::unique_ptr<AbiCompatibilityChecker>()>> abiCompatibilityCheckerMap;
  std::set<std::string> checkerTypes;

public:
  AbiCheckerFactory();
  AbiCheckerFactory(const AbiCheckerFactory &) = default;
  AbiCheckerFactory(AbiCheckerFactory &&) = default;
  AbiCheckerFactory &operator=(const AbiCheckerFactory &) = default;
  AbiCheckerFactory &operator=(AbiCheckerFactory &&) = default;
  ~AbiCheckerFactory() = default;

  std::unique_ptr<AbiCompatibilityChecker> create(const std::string& Type);
  const std::set<std::string>& getCheckerTypes() const { return checkerTypes; }
};

} // namespace mpi
} // namespace hooks
} // namespace sarus

#endif // sarus_hooks_mpi_AbiChecker_hpp