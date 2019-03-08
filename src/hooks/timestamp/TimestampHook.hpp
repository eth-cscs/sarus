#ifndef sarus_hooks_TimestampHook_hpp
#define sarus_hooks_TimestampHook_hpp

#include <boost/filesystem.hpp>
#include <string>


namespace sarus {
namespace hooks {
namespace timestamp {

class TimestampHook {
public:
    TimestampHook(std::string message)
        : message{message}
    {}

    void activate();

private:
    void parseConfigJSONOfBundle();
    void parseEnvironmentVariables();
    void timestamp();
//    std::vector<boost::filesystem::path> parseListOfPaths(const std::string& s);
//    void getContainerLibrariesFromDynamicLinker();
//    void replaceMpiLibrariesInContainer() const;
//    void mountDependencyLibrariesIntoContainer() const;
//    void performBindMounts() const;
//    bool areLibrariesABICompatible(const boost::filesystem::path& hostLib, const boost::filesystem::path& containerLib) const;
//    bool isLibraryInDefaultLinkerDirectory(const boost::filesystem::path& lib) const;
//    void createSymlinksInDefaultLinkerDirectory(const boost::filesystem::path& lib) const;
//    std::tuple<int, int, int> getVersionNumbersOfLibrary(const boost::filesystem::path& lib) const;
//    std::string getStrippedLibraryName(const boost::filesystem::path& path) const;

private:
    bool isHookEnabled{ false };
    std::string message;
    boost::filesystem::path logFilePath;
    boost::filesystem::path bundleDir;
//    boost::filesystem::path rootfsDir;
    pid_t pidOfContainer;
    uid_t uidOfUser;
    gid_t gidOfUser;
};

}}} // namespace

#endif
