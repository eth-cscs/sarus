#ifndef sarus_common_Lockfile_hpp
#define sarus_common_Lockfile_hpp

#include <limits>

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>


namespace sarus {
namespace common {

class Logger;

/**
 * This class provides exclusive access to a shared resource on the filesystem.
 * The constructor attempts to acquire exclusive access to the shared resource by
 * atomically creating a lock file on the filesystem. If a lock file already exists,
 * i.e. the resource was already acquired by somebody else, the constructor
 * busy waits until the lock file is removed.
 * The destructor releases exclusive access to the shared resource by removing
 * the lock file from the filesystem.
 */
class Lockfile {
public:
    static constexpr unsigned int noTimeout = std::numeric_limits<unsigned int>::max();

public:
    Lockfile();
    Lockfile(const boost::filesystem::path& file, unsigned int timeoutMs=noTimeout);
    Lockfile(const Lockfile&) = delete;
    Lockfile(Lockfile&&);
    Lockfile& operator=(const Lockfile&) = delete;
    Lockfile& operator=(Lockfile&&);
    ~Lockfile();

private:
    boost::filesystem::path convertToLockfile(const boost::filesystem::path& file) const;
    bool createLockfileAtomically() const;

private:
    common::Logger* logger;
    std::string loggerSubsystemName = "Lockfile";
    boost::optional<boost::filesystem::path> lockfile;
};

}
}

#endif
