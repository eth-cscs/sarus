#ifndef sarus_hooks_TimestampHook_hpp
#define sarus_hooks_TimestampHook_hpp

#include <boost/filesystem.hpp>
#include <string>


namespace sarus {
namespace hooks {
namespace timestamp {

class TimestampHook {
public:
    void activate();

private:
    void parseConfigJSONOfBundle();
    void parseEnvironmentVariables();
    void timestamp();

private:
    bool isHookEnabled{ false };
    std::string message;
    boost::filesystem::path logFilePath;
    boost::filesystem::path bundleDir;
    pid_t pidOfContainer;
    uid_t uidOfUser;
    gid_t gidOfUser;
};

}}} // namespace

#endif
