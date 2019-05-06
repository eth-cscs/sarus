#include "Utility.hpp"

#include <iostream>
#include <sys/stat.h>
#include <fcntl.h>
#include <boost/format.hpp>
#include <rapidjson/istreamwrapper.h>

#include "common/Error.hpp"


namespace rj = rapidjson;

namespace sarus {
namespace hooks {
namespace common {
namespace utility {

std::tuple<boost::filesystem::path, pid_t> parseStateOfContainerFromStdin() {
    rj::Document state;
    try {
        rj::IStreamWrapper sw(std::cin);
        state.ParseStream(sw);
    }
    catch(const std::exception& e) {
        SARUS_RETHROW_ERROR(e, "Failed to parse container's state JSON from stdin.");
    }

    boost::filesystem::path bundleDir = state["bundle"].GetString();
    pid_t pidOfContainer = state["pid"].GetInt();

    return std::tuple<boost::filesystem::path, pid_t>{bundleDir, pidOfContainer};
}

static void enterNamespace(const boost::filesystem::path& namespaceFile) {
    // get namespace's fd   
    auto fd = open(namespaceFile.c_str(), O_RDONLY);
    if (fd == -1) {
        auto message = boost::format("Failed to open namespace file %s: %s") % namespaceFile % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    // enter namespace
    if (setns(fd, 0) != 0) {
        auto message = boost::format("Failed to enter namespace %s: %s") % namespaceFile % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}

void enterNamespacesOfProcess(pid_t pid) {
    {
        auto file = boost::format("/proc/%s/ns/mnt") % pid;
        enterNamespace(file.str());
    }
    {
        auto file = boost::format("/proc/%s/ns/pid") % pid;
        enterNamespace(file.str());
    }
}

} // utility namespace

namespace test {

rj::Document createBaseConfigJSON(const boost::filesystem::path& rootfsDir, const std::tuple<uid_t, gid_t>& idsOfUser) {
    auto doc = rj::Document{rj::kObjectType};
    auto& allocator = doc.GetAllocator();

    // root
    doc.AddMember(
        "root",
        rj::Value{rj::kObjectType},
        allocator);
    doc["root"].AddMember(
        "path",
        rj::Value{rootfsDir.filename().c_str(), allocator},
        allocator);

    // process
    doc.AddMember(
        "process",
        rj::Document{rj::kObjectType},
        allocator);
    doc["process"].AddMember(
        "user",
        rj::Document{rj::kObjectType},
        allocator);
    doc["process"]["user"].AddMember(
        "uid",
        rj::Value{std::get<0>(idsOfUser)},
        allocator);
    doc["process"]["user"].AddMember(
        "gid",
        rj::Value{std::get<1>(idsOfUser)},
        allocator);
    doc["process"].AddMember(
        "env",
        rj::Document{rj::kArrayType},
        allocator);

    return doc;
}

}}}} // namespace
