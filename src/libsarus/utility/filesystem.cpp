/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "filesystem.hpp"

#include <fstream>
#include <sys/stat.h>

#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

#include "libsarus/Error.hpp"
#include "libsarus/utility/logging.hpp"
#include "libsarus/utility/string.hpp"

/**
 * Utility functions for filesystem manipulation 
 */

namespace libsarus {
namespace filesystem {

size_t getFileSize(const boost::filesystem::path& filename) {
    struct stat st;
    if(stat(filename.c_str(), &st) != 0) {
        auto message = boost::format("Failed to retrieve size of file %s. Stat failed: %s")
            % filename % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    return st.st_size;
}

std::tuple<uid_t, gid_t> getOwner(const boost::filesystem::path& path) {
    struct stat sb;
    if(stat(path.c_str(), &sb) != 0) {
        auto message = boost::format("Failed to retrieve owner of file %s. Stat failed: %s")
            % path % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    return std::tuple<uid_t, gid_t>{sb.st_uid, sb.st_gid};
}

void setOwner(const boost::filesystem::path& path, uid_t uid, gid_t gid) {
    if(uid == static_cast<uid_t>(-1) || gid==static_cast<uid_t>(-1)) {
        assert( uid == static_cast<uid_t>(-1)
            && gid == static_cast<gid_t>(-1));
        return;
    }

    if(!boost::filesystem::exists(path)) {
        auto message = boost::format("attempted to change ownership of non existing path %s") % path;
        SARUS_THROW_ERROR(message.str());
    }

    if(chown(path.c_str(), uid, gid) != 0) {
        auto errorMessage = boost::format("failed to change ownership of path: %s") % path;
    }
}

void createFoldersIfNecessary(const boost::filesystem::path& path, uid_t uid, gid_t gid) {
    auto currentPath = boost::filesystem::path("");

    if(!boost::filesystem::exists(path)) {
        logMessage(boost::format{"Creating directory %s"} % path, LogLevel::DEBUG);
    }

    for(const auto& element : path) {
        currentPath /= element;
        if(!boost::filesystem::exists(currentPath)) {
            bool created = false;
            try {
                created = boost::filesystem::create_directory(currentPath);
            } catch(const std::exception& e) {
                auto message = boost::format("Failed to create directory %s") % currentPath;
                SARUS_RETHROW_ERROR(e, message.str());
            }
            if(!created) {
                // the creation might have failed because another process concurrently
                // created the same directory. So check whether the directory was indeed
                // created by another process.
                if(!boost::filesystem::is_directory(currentPath)) {
                    auto message = boost::format("Failed to create directory %s") % currentPath;
                    SARUS_THROW_ERROR(message.str());
                }
            }
            setOwner(currentPath, uid, gid);
        }
    }
}

void createFileIfNecessary(const boost::filesystem::path& path, uid_t uid, gid_t gid) {
    // NOTE: Broken symlinks will NOT be recognized as existing and hence will be overridden.
    if(boost::filesystem::exists(path)){
        logMessage(boost::format{"File %s already exists"} % path, LogLevel::DEBUG);
        return;
    }

    logMessage(boost::format{"Creating file %s"} % path, LogLevel::DEBUG);
    if(!boost::filesystem::exists(path.parent_path())) {
        createFoldersIfNecessary(path.parent_path(), uid, gid);
    }
    std::ofstream of(path.c_str());
    if(!of.is_open()) {
        auto message = boost::format("Failed to create file %s") % path;
        SARUS_THROW_ERROR(message.str());
    }
    setOwner(path, uid, gid);
}

void copyFile(const boost::filesystem::path& src, const boost::filesystem::path& dst, uid_t uid, gid_t gid) {
    logMessage(boost::format{"Copying %s -> %s"} % src % dst, LogLevel::DEBUG);
    createFoldersIfNecessary(dst.parent_path(), uid, gid);
    boost::filesystem::remove(dst); // remove dst if already exists
    boost::filesystem::copy_file(src, dst);
    setOwner(dst, uid, gid);
}

void removeFile(const boost::filesystem::path& path) {
    if(boost::filesystem::exists(path)) {
        boost::filesystem::remove(path);
    }
}

void copyFolder(const boost::filesystem::path& src, const boost::filesystem::path& dst, uid_t uid, gid_t gid) {
    if(!boost::filesystem::exists(src) || !boost::filesystem::is_directory(src)) {
        auto message = boost::format("Failed to copy %s to %s: source folder doesn't exist.") % src % dst;
        SARUS_THROW_ERROR(message.str());
    }

    if(boost::filesystem::exists(dst)) {
        auto message = boost::format("Failed to copy %s to %s: destination already exists.") % src % dst;
        SARUS_THROW_ERROR(message.str());
    }

    filesystem::createFoldersIfNecessary(dst, uid, gid);

    // for each file/folder in the directory
    for(boost::filesystem::directory_iterator entry{src};
        entry != boost::filesystem::directory_iterator{};
        ++entry) {
        if(boost::filesystem::is_directory(entry->path())) {
            copyFolder(entry->path(), dst / entry->path().filename(), uid, gid);
        }
        else {
            copyFile(entry->path(), dst / entry->path().filename(), uid, gid);
        }
    }
}

void changeDirectory(const boost::filesystem::path& path) {
    if(!boost::filesystem::exists(path)) {
        auto message = boost::format("attemped to cd into %s, but directory doesn't exist") % path;
        SARUS_THROW_ERROR(message.str());
    }

    if(chdir(path.string().c_str()) != 0) {
        auto message = boost::format("failed to cd into %s: %s") % path % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
}

int countFilesInDirectory(const boost::filesystem::path& path) {
    if (!boost::filesystem::exists(path) || !boost::filesystem::is_directory(path)) {
        auto message = boost::format("Failed to count files in %s: path is not an existing directory.") % path;
        SARUS_THROW_ERROR(message.str());
    }

    auto path_iterator = boost::filesystem::directory_iterator(path);

    // The directory_iterator() ctor without arguments always creates the "end" iterator
    auto numberOfFiles = static_cast<int>(std::distance(path_iterator, boost::filesystem::directory_iterator()));
    return numberOfFiles;
}

std::string readFile(const boost::filesystem::path& path) {
    std::ifstream ifs(path.string());
    auto s = std::string(   std::istreambuf_iterator<char>(ifs),
                            std::istreambuf_iterator<char>());
    return s;
}

void writeTextFile(const std::string& text, const boost::filesystem::path& filename, const std::ios_base::openmode mode) {
    try {
        createFoldersIfNecessary(filename.parent_path());
        auto ofs = std::ofstream{filename.string(), mode};
        if (!ofs) {
            auto message = boost::format("Failed to open std::ofstream for %s") % filename;
            SARUS_THROW_ERROR(message.str());
        }
        ofs << text;
    }
    catch(const std::exception& e) {
        auto message = boost::format("Failed to write text file %s") % filename;
        SARUS_RETHROW_ERROR(e, message.str());
    }
}

/**
 * Generates a random suffix and append it to the given path. If the generated random
 * path exists, tries again with another suffix until the operation succeedes.
 *
 * Note: boost::filesystem::unique_path offers a similar functionality. However, it
 * fails (throws exception) when the locale configuration is invalid. More specifically,
 * we experienced the problem when LC_CTYPE was set to UTF-8 and the locale UTF-8 was not
 * installed.
 */
boost::filesystem::path makeUniquePathWithRandomSuffix(const boost::filesystem::path& path) {
    auto uniquePath = std::string{};

    do {
        const size_t sizeOfRandomSuffix = 16;
        uniquePath = path.string() + "-" + string::generateRandom(sizeOfRandomSuffix);
    } while(boost::filesystem::exists(uniquePath));

    return uniquePath;
}

std::string makeColonSeparatedListOfPaths(const std::vector<boost::filesystem::path>& paths) {
    auto s = std::string{};
    bool isFirstPath = true;
    for(const auto& path : paths) {
        if(isFirstPath) {
            isFirstPath = false;
        }
        else {
            s += ":";
        }
        s += path.string();
    }
    return s;
}

static boost::filesystem::path getSymlinkTarget(const boost::filesystem::path& path) {
    char buffer[PATH_MAX];
    auto count = readlink(path.string().c_str(), buffer, PATH_MAX);
    assert(count < PATH_MAX); // PATH_MAX is supposed to be large enough for any path + NULL terminating char
    buffer[count] = '\0';
    return buffer;
}

/*
    Appends path1 to path0 resolving symlinks within rootfs. E.g.:

    rootfs = /rootfs
    path0 = /etc
    path1 = sarus/sarus.json

    and in rootfs we have:

    /rootfs/etc/sarus -> /etc/sarus-1.0
    /rootfs/etc/sarus-1.0/sarus.json -> sarus-1.0.json

    then:

    result = /etc/sarus-1.0/sarus-1.0.json

    At the end of the function execution, the optional output parameter 'traversedSymlinks'
    contains the various symlinks that were traversed during the path resolution process.

    NOTE: (from lee) This function was exported via a header file because while 
    its operation falls in the "path" category (Path.hpp), the function itself 
    was also used by the "shared library" category (SharedLibs.hpp).
*/
boost::filesystem::path appendPathsWithinRootfs( const boost::filesystem::path& rootfs,
                                                 const boost::filesystem::path& path0,
                                                 const boost::filesystem::path& path1,
                                                 std::vector<boost::filesystem::path>* traversedSymlinks) {
    auto current = path0;

    for(const auto& element : path1) {
        if(element == "/") {
            continue;
        }
        else if(element == ".") {
            continue;
        }
        else if(element == "..") {
            if(current > "/") {
                current = current.remove_trailing_separator().parent_path();
            }
        }
        else if(isSymlink(rootfs / current / element)) {
            if(traversedSymlinks) {
                traversedSymlinks->push_back(current / element);
            }
            auto target = getSymlinkTarget(rootfs / current / element);
            if(target.is_absolute()) {
                current = appendPathsWithinRootfs(rootfs, "/", target, traversedSymlinks);
            }
            else {
                current = appendPathsWithinRootfs(rootfs, current, target, traversedSymlinks);
            }
        }
        else {
            current /= element;
        }
    }

    return current;
}

boost::filesystem::path realpathWithinRootfs(const boost::filesystem::path& rootfs, const boost::filesystem::path& path) {
    if(!path.is_absolute()) {
        auto message = boost::format("Failed to determine realpath within rootfs. %s is not an absolute path.") % path;
        SARUS_THROW_ERROR(message.str());
    }

    return appendPathsWithinRootfs(rootfs, "/", path);
}

dev_t getDeviceID(const boost::filesystem::path& path) {
    struct stat sb;
    if(stat(path.c_str(), &sb) != 0) {
        auto message = boost::format("Failed to retrieve device ID of file %s. Stat failed: %s")
            % path % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    logMessage(boost::format("Got device ID for %s: %d") % path % sb.st_rdev, LogLevel::DEBUG);
    return sb.st_rdev;
}

char getDeviceType(const boost::filesystem::path& path) {
    char deviceType;
    if (filesystem::isCharacterDevice(path)) {
        deviceType = 'c';
    }
    else if (filesystem::isBlockDevice(path)) {
        deviceType = 'b';
    }
    else {
        auto message = boost::format("Failed to recognize device type of file %s."
                                     " File is not a device or has unknown device type.") % path;
        SARUS_THROW_ERROR(message.str());
    }
    logMessage(boost::format("Got device type for %s: '%c'") % path % deviceType, LogLevel::DEBUG);
    return deviceType;
}

bool isDeviceFile(const boost::filesystem::path& path) {
    struct stat sb;
    if(stat(path.c_str(), &sb) != 0) {
        auto message = boost::format("Failed to check if file %s is a device file. Stat failed: %s")
            % path % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    return S_ISBLK(sb.st_mode) || S_ISCHR(sb.st_mode);
}

bool isBlockDevice(const boost::filesystem::path& path) {
    struct stat sb;
    if(stat(path.c_str(), &sb) != 0) {
        auto message = boost::format("Failed to check if file %s is a block device. Stat failed: %s")
            % path % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    return S_ISBLK(sb.st_mode);
}

bool isCharacterDevice(const boost::filesystem::path& path) {
    struct stat sb;
    if (stat(path.c_str(), &sb) != 0) {
        auto message = boost::format("Failed to check if file %s is a charater device. Stat failed: %s")
            % path % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    return S_ISCHR(sb.st_mode);
}

bool isSymlink(const boost::filesystem::path& path) {
    struct stat sb;
    if (lstat(path.c_str(), &sb) != 0) {
        return false;
    }
    return S_ISLNK(sb.st_mode);
}

bool isLibc(const boost::filesystem::path& lib) {
    boost::cmatch matches;
    boost::regex re("^(.*/)*libc(-\\d+\\.\\d+)?\\.so(.\\d+)?$");
    if (boost::regex_match(lib.c_str(), matches, re)) {
        return true;
    }
    else {
        return false;
    }
}

bool isSharedLib(const boost::filesystem::path& file) {
    auto filename = file.filename().string();

    // check that is not a directory, e.g. /etc/ld.so.conf.d
    if(boost::filesystem::is_directory(file)){
        return false;
    }

    // check that extension doesn't end with '.conf', e.g. /etc/ld.so.conf
    if(boost::ends_with(filename, ".conf")) {
        return false;
    }

    // check that extension doesn't end with '.cache', e.g. /etc/ld.so.cache
    if(boost::ends_with(filename, ".cache")) {
        return false;
    }

    // check that extension contains '.so'
    auto extension = std::string{".so"};
    auto pos = std::find_end(filename.cbegin(), filename.cend(), extension.cbegin(), extension.cend());
    if(pos == filename.cend()) {
        return false;
    }
    return pos+3 == filename.cend() || *(pos+3) == '.';
}

}}
