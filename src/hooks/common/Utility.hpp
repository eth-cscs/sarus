#ifndef sarus_hooks_common_Utility_hpp
#define sarus_hooks_common_Utility_hpp

#include <tuple>
#include <sys/types.h>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

namespace sarus {
namespace hooks {
namespace common {
namespace utility {

std::tuple<boost::filesystem::path, pid_t> parseStateOfContainerFromStdin();
void enterNamespacesOfProcess(pid_t);

} // namespace utility

namespace test {

rapidjson::Document createBaseConfigJSON(const boost::filesystem::path& rootfsDir, const std::tuple<uid_t, gid_t>& idsOfUser);

}}}} // namespace

#endif
