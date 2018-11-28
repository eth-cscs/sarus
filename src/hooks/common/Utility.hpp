#ifndef sarus_hooks_common_Utility_hpp
#define sarus_hooks_common_Utility_hpp

#include <tuple>
#include <sys/types.h>
#include <boost/filesystem.hpp>

namespace sarus {
namespace hooks {
namespace common {
namespace utility {

std::tuple<boost::filesystem::path, pid_t> parseStateOfContainerFromStdin();
void enterNamespacesOfProcess(pid_t);

}}}} // namespace

#endif
