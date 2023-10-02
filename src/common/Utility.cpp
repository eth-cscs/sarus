/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Utility.hpp"

#include <fstream>
#include <sstream>
#include <array>
#include <memory>
#include <string>
#include <stdexcept>
#include <iterator>
#include <random>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include <wordexp.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/fsuid.h>
#include <sched.h>
#include <pwd.h>
#include <grp.h>
#include <limits.h>
#include <fcntl.h>

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>
#include <rapidjson/writer.h>

#include "common/Config.hpp"
#include "common/Error.hpp"
#include "common/PasswdDB.hpp"

/**
 * Utility functions
 */

namespace sarus {
namespace common {

std::unordered_map<std::string, std::string> parseEnvironmentVariables(char** env) {
    auto map = std::unordered_map<std::string, std::string>{};
    for(size_t i=0; env[i] != nullptr; ++i) {
        std::string key, value;
        std::tie(key, value) = parseEnvironmentVariable(env[i]);
        map[key] = value;
    }
    return map;
}

std::pair<std::string, std::string> parseEnvironmentVariable(const std::string& variable) {
    std::pair<std::string, std::string> kvPair;
    try {
        kvPair = parseKeyValuePair(variable);
    }
    catch(const common::Error& e) {
        auto message = boost::format("Failed to parse environment variable: %s") % e.what();
        SARUS_RETHROW_ERROR(e, message.str());
    }
    return kvPair;
}

std::string getEnvironmentVariable(const std::string& key) {
    char* p;
    if((p = getenv(key.c_str())) == nullptr) {
        auto message = boost::format("Environment doesn't contain variable with key %s") % key;
        SARUS_THROW_ERROR(message.str());
    }
    logMessage(boost::format("Got environment variable %s=%s") % key % p, common::LogLevel::DEBUG);
    return p;
}

void setEnvironmentVariable(const std::string& key, const std::string& value) {
    int overwrite = 1;
    if(setenv(key.c_str(), value.c_str(), overwrite) != 0) {
        auto message = boost::format("Failed to setenv(%s, %s, %d): %s")
            % key % value % overwrite % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    logMessage(boost::format("Set environment variable %s=%s") % key % value, common::LogLevel::DEBUG);
}

std::string removeWhitespaces(const std::string& s) {
    auto result = s;
    auto newEnd = std::remove_if(result.begin(), result.end(), iswspace);
    result.erase(newEnd, result.end());
    return result;
}

std::string replaceString(std::string &buf, const std::string& from, const std::string& to) {
    std::string::size_type pos = buf.find(from);
    while(pos != std::string::npos){
        buf.replace(pos, from.size(), to);
        pos = buf.find(from, pos + to.size());
    }
    return buf;
}

std::string eraseFirstAndLastDoubleQuote(const std::string& s) {
    if(s.size() < 2 || *s.cbegin() != '"' || *s.crbegin() != '"') {
        auto message = boost::format(   "Failed to remove first and last double quotes"
                                        " in string \"%s\". The string doesn't"
                                        " contain such double quotes.") % s;
        SARUS_THROW_ERROR(message.str());
    }
    return std::string( s.cbegin() + 1,
                        s.cbegin() + s.size() - 1);
}

std::pair<std::string, std::string> parseKeyValuePair(const std::string& pairString, const char separator) {
    auto keyEnd = std::find(pairString.cbegin(), pairString.cend(), separator);
    auto key = std::string(pairString.cbegin(), keyEnd);
    auto value = keyEnd != pairString.cend() ? std::string(keyEnd+1, pairString.cend()) : std::string{};
    if(key.empty()) {
        auto message = boost::format("Failed to parse key-value pair '%s': key is empty") % pairString;
        SARUS_THROW_ERROR(message.str())
    }
    return std::pair<std::string, std::string>{key, value};
}

void switchIdentity(const common::UserIdentity& identity) {
    logProcessUserAndGroupIdentifiers();

    logMessage(boost::format{"Switching to identity (uid=%d gid=%d)"}
               % identity.uid % identity.gid,
               LogLevel::DEBUG);

    uid_t euid = geteuid();
    uid_t egid = getegid();

    if (euid == 0){
        // unprivileged processes cannot call setgroups
        if (setgroups(identity.supplementaryGids.size(), identity.supplementaryGids.data()) != 0) {
            SARUS_THROW_ERROR("Failed to setgroups");
        }
    }

    if (setegid(identity.gid) != 0) {
        SARUS_THROW_ERROR("Failed to setegid");
    }

    if (seteuid(identity.uid) != 0) {
        if (setegid(egid) != 0) {
            SARUS_THROW_ERROR("Failed to seteuid and Failed to restore egid");
        }
        SARUS_THROW_ERROR("Failed to seteuid");
    }

    logProcessUserAndGroupIdentifiers();
    logMessage("Successfully switched identity", LogLevel::DEBUG);
}

/*
 * Set the filesystem user ID to the uid in the provided UserIdentity
 *
 * Normally the filesystem user ID (or fsuid for short) coincides with the
 * effective user ID (euid) and is changed by the kernel when the euid is set,
 * as described in the Linux man pages:
 * https://man7.org/linux/man-pages/man2/setfsuid.2.html
 * https://man7.org/linux/man-pages/man7/credentials.7.html
 *
 * However, when having to bind-mount files which reside on root_squashed filesystems,
 * a process needs to have both root privileges (to perform the mount) and normal
 * user filesystem permissions (under root_squash, root is remapped to nobody and
 * cannot access the user content unless said content is world-readable).
 * The above is the main scenario in which this function is meant to be used.
 * Other similar use cases where both root privileges and user permissions are
 * required might occur.
 */
void setFilesystemUid(const common::UserIdentity& identity) {
    logMessage(boost::format{"Setting filesystem uid to %d"} % identity.uid,
               LogLevel::DEBUG);

    setfsuid(identity.uid);
    if (setfsuid(identity.uid) != int(identity.uid)){
        SARUS_THROW_ERROR("Failed to set filesystem uid");
    }

    logMessage("Successfully set filesystem uid", LogLevel::DEBUG);
}

void logProcessUserAndGroupIdentifiers() {
    uid_t ruid, euid, suid;
    if (getresuid(&ruid, &euid, &suid) != 0){
        SARUS_THROW_ERROR("getresuid failed");
    }
    gid_t rgid, egid, sgid;
    if (getresgid(&rgid, &egid, &sgid) != 0){
        SARUS_THROW_ERROR("getresgid failed");
    }
    logMessage(boost::format("Current uids (r/e/s/fs): %d %d %d %d")
               % ruid % euid % suid % setfsuid(-1), common::LogLevel::DEBUG);
    logMessage(boost::format("Current gids (r/e/s/fs): %d %d %d %d")
               % rgid % egid % sgid % setfsgid(-1), common::LogLevel::DEBUG);
}

std::string executeCommand(const std::string& command) {
    auto commandWithRedirection = command + " 2>&1"; // stderr-to-stdout redirection necessary because popen only reads stdout
    logMessage(boost::format("Executing command '%s'") % commandWithRedirection, common::LogLevel::DEBUG);

    FILE* pipe = popen(commandWithRedirection.c_str(), "r");
    if(!pipe) {
        auto message = boost::format("Failed to execute command \"%s\". Call to popen() failed (%s)")
            % commandWithRedirection % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    char buffer[1024];
    std::string commandOutput;
    while(!feof(pipe)) {
        if(fgets(buffer, sizeof(buffer), pipe)) {
            commandOutput += buffer;
        }
        else if(!feof(pipe)) {
            auto message = boost::format("Failed to execute command \"%s\". Call to fgets() failed.")
                % commandWithRedirection;
            SARUS_THROW_ERROR(message.str());
        }
    }

    auto status = pclose(pipe);
    if(status == -1) {
        auto message = boost::format("Failed to execute command \"%s\". Call to pclose() failed (%s)")
            % commandWithRedirection % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    else if(!WIFEXITED(status)) {
        auto message = boost::format(   "Failed to execute command \"%s\"."
                                        " Process terminated abnormally. Process' output:\n\n%s")
                                        % commandWithRedirection % commandOutput;
        SARUS_THROW_ERROR(message.str());
    }
    else if(WEXITSTATUS(status) != 0) {
        auto message = boost::format(   "Failed to execute command \"%s\"."
                                        " Process terminated with status %d. Process' output:\n\n%s")
                                        % commandWithRedirection % WEXITSTATUS(status) % commandOutput;
        SARUS_THROW_ERROR(message.str());
    }

    return commandOutput;
}

int forkExecWait(const common::CLIArguments& args,
                 const boost::optional<std::function<void()>>& preExecChildActions,
                 const boost::optional<std::function<void(int)>>& postForkParentActions) {
    logMessage(boost::format("Forking and executing '%s'") % args, common::LogLevel::DEBUG);

    // fork and execute
    auto pid = fork();
    if(pid == -1) {
        auto message = boost::format("Failed to fork to execute subprocess %s: %s")
            % args % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    bool isChild = pid == 0;
    if(isChild) {
        if(preExecChildActions) {
            (*preExecChildActions)();
        }
        execvp(args.argv()[0], args.argv());
        auto message = boost::format("Failed to execvp subprocess %s: %s") % args % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    else {
        if(postForkParentActions) {
            (*postForkParentActions)(pid);
        }
        int status;
        do {
            if(waitpid(pid, &status, 0) == -1) {
                auto message = boost::format("Failed to waitpid subprocess %s: %s")
                    % args % strerror(errno);
                SARUS_THROW_ERROR(message.str());
            }
        } while(!WIFEXITED(status) && !WIFSIGNALED(status));

        if(!WIFEXITED(status)) {
            auto message = boost::format("Subprocess %s terminated abnormally")
                % args;
            SARUS_THROW_ERROR(message.str());
        }

        logMessage( boost::format("%s exited with status %d") % args % WEXITSTATUS(status),
                    common::LogLevel::DEBUG);

        return WEXITSTATUS(status);
    }
}

void redirectStdoutToFile(const boost::filesystem::path& path) {
    int fd = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    dup2(fd, 1);
    close(fd);
}

void SetStdinEcho(bool flag)
{
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    if( !flag ) {
        tty.c_lflag &= ~ECHO;
    }
    else {
        tty.c_lflag |= ECHO;
    }

    (void) tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

std::string getHostname() {
    char hostname[HOST_NAME_MAX];
    if(gethostname(hostname, HOST_NAME_MAX) != 0) {
        auto message = boost::format("failed to retrieve hostname (%s)") % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    hostname[HOST_NAME_MAX-1] = '\0';
    return hostname;
}

size_t getFileSize(const boost::filesystem::path& filename) {
    struct stat st;
    if(stat(filename.c_str(), &st) != 0) {
        auto message = boost::format("Failed to retrieve size of file %s. Stat failed: %s")
            % filename % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    return st.st_size;
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
    if (sarus::common::isCharacterDevice(path)) {
        deviceType = 'c';
    }
    else if (sarus::common::isBlockDevice(path)) {
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

bool isCentralizedRepositoryEnabled(const common::Config& config) {
    // centralized repository is enabled when a directory is specified
    return config.json.HasMember("centralizedRepositoryDir");
}

boost::filesystem::path getCentralizedRepositoryDirectory(const common::Config& config) {
    if(!isCentralizedRepositoryEnabled(config)) {
        SARUS_THROW_ERROR("failed to retrieve directory of centralized repository"
                            " because such feature is disabled. Please ask your system"
                            " administrator to enable the central read-only repository.");
    }
    return config.json["centralizedRepositoryDir"].GetString();
}

boost::filesystem::path getLocalRepositoryDirectory(const common::Config& config) {
    auto baseDir = boost::filesystem::path{ config.json["localRepositoryBaseDir"].GetString() };
    auto passwdFile = boost::filesystem::path{ config.json["prefixDir"].GetString() } / "etc/passwd";
    auto username = PasswdDB{passwdFile}.getUsername(config.userIdentity.uid);
    return baseDir / username / common::Config::BuildTime{}.localRepositoryFolder;
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
        uniquePath = path.string() + "-" + generateRandomString(sizeOfRandomSuffix);
    } while(boost::filesystem::exists(uniquePath));

    return uniquePath;
}

std::string generateRandomString(size_t size) {
    auto dist = std::uniform_int_distribution<std::mt19937::result_type>(0, 'z'-'a');
    std::mt19937 generator;
    generator.seed(std::random_device()());

    auto string = std::string(size, '.');

    for(size_t i=0; i<string.size(); ++i) {
        auto randomCharacter = 'a' + dist(generator);
        string[i] = randomCharacter;
    }

    return string;
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

void copyFolder(const boost::filesystem::path& src, const boost::filesystem::path& dst, uid_t uid, gid_t gid) {
    if(!boost::filesystem::exists(src) || !boost::filesystem::is_directory(src)) {
        auto message = boost::format("Failed to copy %s to %s: source folder doesn't exist.") % src % dst;
        SARUS_THROW_ERROR(message.str());
    }

    if(boost::filesystem::exists(dst)) {
        auto message = boost::format("Failed to copy %s to %s: destination already exists.") % src % dst;
        SARUS_THROW_ERROR(message.str());
    }

    common::createFoldersIfNecessary(dst, uid, gid);

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
*/
static boost::filesystem::path appendPathsWithinRootfs( const boost::filesystem::path& rootfs,
                                                        const boost::filesystem::path& path0,
                                                        const boost::filesystem::path& path1,
                                                        std::vector<boost::filesystem::path>* traversedSymlinks = nullptr) {
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

/**
 * Converts a string representing a list of key-value pairs to a map.
 *
 * If no separators are passed as arguments, the pairs are assumed to be separated by commas,
 * while keys and values are assumed to be separated by an = sign.
 * If a value is not specified (i.e. a character sequence between two pair separators does
 * not feature a key-value separator), the map entry is created with the value as an
 * empty string.
 */
std::unordered_map<std::string, std::string> parseMap(const std::string& input,
                                                      const char pairSeparators,
                                                      const char keyValueSeparators) {
    if(input.empty()) {
        return std::unordered_map<std::string, std::string>{};
    }

    auto map = std::unordered_map<std::string, std::string>{};

    auto pairs = std::vector<std::string>{};
    boost::split(pairs, input, boost::is_any_of(std::string{pairSeparators}));

    for(const auto& pair : pairs) {
        std::string key, value;
        try {
            std::tie(key, value) = common::parseKeyValuePair(pair, keyValueSeparators);
        }
        catch(std::exception& e) {
            auto message = boost::format("Error parsing '%s'. %s") % input % e.what();
            logMessage(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }

        // do not allow repeated separators in the value
        auto valueEnd = std::find(value.cbegin(), value.cend(), keyValueSeparators);
        if(valueEnd != value.cend()) {
            auto message = boost::format("Error parsing '%s'. Invalid key-value pair '%s': repeated use of separator is not allowed.")
                % input % pair;
            logMessage(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO)
        }

        // check for duplicated key
        if(map.find(key) != map.cend()) {
            auto message = boost::format("Error parsing '%s'. Found duplicated key '%s': expected a list of unique key-value pairs.")
                % input % key;
            logMessage(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO)
        }

        map[key] = value;
    }

    return map;
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

boost::filesystem::path getSharedLibLinkerName(const boost::filesystem::path& path) {
    auto extension = std::string{".so"};
    auto filename = path.filename().string();
    auto dot = std::find_end(filename.begin(), filename.end(), extension.cbegin(), extension.cend());

    if(dot == filename.cend() || (dot+3 != filename.cend() && *(dot+3) != '.')) {
        auto message = boost::format{"Failed to parse linker name from invalid library path '%s'"} % path;
        SARUS_THROW_ERROR(message.str());
    }

    filename.erase(dot+3, filename.end());
    return filename;
}

std::vector<boost::filesystem::path> getSharedLibsFromDynamicLinker(
    const boost::filesystem::path& ldconfigPath,
    const boost::filesystem::path& rootDir) {

    auto libraries = std::vector<boost::filesystem::path>{};
    auto command = boost::format("%s -r %s -p") % ldconfigPath.string() % rootDir.string();
    auto output = sarus::common::executeCommand(command.str());
    std::stringstream stream{output};
    std::string line;
    while(std::getline(stream, line)) {
        // Look for "arrow" separator to only parse lines containing library entries
        auto pos = line.rfind(" => ");
        if(pos == std::string::npos) {
            continue;
        }
        auto library = line.substr(pos + 4);
        libraries.push_back(library);
    }

    return libraries;
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

std::vector<std::string> parseSharedLibAbi(const boost::filesystem::path& lib) {
    if(!isSharedLib(lib)) {
        auto message = boost::format{"Cannot parse ABI version of '%s': not a shared library"} % lib;
        SARUS_THROW_ERROR(message.str());
    }

    auto name = lib.filename().string();
    auto extension = std::string{".so"};

    auto pos = std::find_end(name.cbegin(), name.cend(), extension.cbegin(), extension.cend());

    if(pos == name.cend()) {
        auto message = boost::format("Failed to get version numbers of library %s."
                                     " Expected a library with file extension '%s'.") % lib % extension;
        SARUS_THROW_ERROR(message.str());
    }

    if(pos+3 == name.cend()) {
        return {};
    }

    auto tokens = std::vector<std::string>{};
    auto versionString = std::string(pos+4, name.cend());
    boost::split(tokens, versionString, boost::is_any_of("."));

    return tokens;
}

std::vector<std::string> resolveSharedLibAbi(const boost::filesystem::path& lib,
                                             const boost::filesystem::path& rootDir) {
    if(!isSharedLib(lib)) {
        auto message = boost::format{"Cannot resolve ABI version of '%s': not a shared library"} % lib;
        SARUS_THROW_ERROR(message.str());
    }

    auto longestAbiSoFar = std::vector<std::string>{};

    auto traversedSymlinks = std::vector<boost::filesystem::path>{};
    auto libReal = appendPathsWithinRootfs(rootDir, "/", lib, &traversedSymlinks);
    auto pathsToProcess = std::move(traversedSymlinks);
    pathsToProcess.push_back(std::move(libReal));

    for(const auto& path : pathsToProcess) {
        if(!isSharedLib(path)) {
            // some traversed symlinks may not be library filenames,
            // e.g. with /lib -> /lib64
            continue;
        }
        if(getSharedLibLinkerName(path) != getSharedLibLinkerName(lib)) {
            // E.g. on Cray we could have:
            // mpich-gnu-abi/7.1/lib/libmpi.so.12 -> ../../../mpich-gnu/7.1/lib/libmpich_gnu_71.so.3.0.1
            // Let's ignore the symlink's target in this case
            auto message = boost::format("Failed to resolve ABI version of\n%s -> %s\nThe symlink"
                                        " and the target library have incompatible linker names. Assuming the symlink is correct.")
                                        % lib % path;
            logMessage(message.str(), LogLevel::DEBUG);
            continue;
        }

        auto abi = parseSharedLibAbi(path);

        const auto& shorter = abi.size() < longestAbiSoFar.size() ? abi : longestAbiSoFar;
        const auto& longer = abi.size() > longestAbiSoFar.size() ? abi : longestAbiSoFar;

        if(!std::equal(shorter.cbegin(), shorter.cend(), longer.cbegin())) {
            // Some vendors have symlinks with incompatible major versions. e.g.
            // libvdpau_nvidia.so.1 -> libvdpau_nvidia.so.440.33.01.
            // For these cases, we trust the vendor and resolve the Lib Abi to that of the symlink.
            auto message = boost::format("Failed to resolve ABI version of\n%s -> %s\nThe symlink filename"
                                        " and the target library have incompatible ABI versions. Assuming symlink is correct.")
                                        % lib % path;
            logMessage(message.str(), LogLevel::DEBUG);
            continue;
        }

        longestAbiSoFar = std::move(longer);
    }

    return longestAbiSoFar;
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

std::string getSharedLibSoname(const boost::filesystem::path& path, const boost::filesystem::path& readelfPath) {
    auto command = boost::format("%s -d %d") % readelfPath % path;
    auto output = sarus::common::executeCommand(command.str());

    std::stringstream stream{output};
    std::string line;
    while(std::getline(stream, line)) {
        boost::cmatch matches;
        boost::regex re("^.* \\(SONAME\\) +Library soname: \\[(.*)\\]$");
        if (boost::regex_match(line.c_str(), matches, re)) {
            return matches[1];
        }
    }

    auto message = boost::format("Failed to parse library soname from readelf output: %s") % output;
    SARUS_THROW_ERROR(message.str());
}

bool is64bitSharedLib(const boost::filesystem::path& path, const boost::filesystem::path& readelfPath) {
    boost::cmatch matches;
    boost::regex re("^ *Machine: +Advanced Micro Devices X86-64 *$");

    auto command = boost::format("%s -h %s") % readelfPath.string() % path.string();
    auto output = sarus::common::executeCommand(command.str());

    std::stringstream stream{output};
    std::string line;
    while(std::getline(stream, line)) {
        if (boost::regex_match(line.c_str(), matches, re)) {
            return true;
        }
    }

    return false;
}

std::vector<int> getCpuAffinity() {
    logMessage("Getting CPU affinity (list of CPU ids)", common::LogLevel::INFO);

    auto set = cpu_set_t{};

    if(sched_getaffinity(getpid(), sizeof(cpu_set_t), &set) != 0) {
        auto message = boost::format("sched_getaffinity failed: %s ") % std::strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    auto cpus = std::vector<int>{};

    for(int cpu=0; cpu<CPU_SETSIZE; ++cpu) {
        if(CPU_ISSET(cpu, &set)) {
            cpus.push_back(cpu);
            logMessage(boost::format("Detected CPU %d") % cpu, common::LogLevel::DEBUG);
        }
    }

    logMessage("Successfully got CPU affinity", common::LogLevel::INFO);

    return cpus;
}

void setCpuAffinity(const std::vector<int>& cpus) {
    logMessage("Setting CPU affinity", common::LogLevel::INFO);

    auto set = cpu_set_t{};
    CPU_ZERO(&set);

    for(auto cpu : cpus) {
        CPU_SET(cpu, &set);
        logMessage(boost::format("Set CPU %d") % cpu, common::LogLevel::DEBUG);
    }

    if(sched_setaffinity(getpid(), sizeof(cpu_set_t), &set) != 0) {
        auto message = boost::format{"sched_setaffinity failed: %s"} % std::strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    logMessage("Successfully set CPU affinity", common::LogLevel::INFO);
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

rapidjson::Document parseJSONStream(std::istream& is) {
    auto json = rapidjson::Document{};

    try {
        rapidjson::IStreamWrapper isw(is);
        json.ParseStream(isw);
    }
    catch (const std::exception& e) {
        SARUS_RETHROW_ERROR(e, "Error parsing JSON stream");
    }

    return json;
}

rapidjson::Document parseJSON(const std::string& string) {
    std::istringstream iss(string);
    auto json = parseJSONStream(iss);
    if (json.HasParseError()) {
        auto message = boost::format(
            "Error parsing JSON string:\n'%s'\nInput data is not valid JSON\n"
            "Error(offset %u): %s")
            % string
            % static_cast<unsigned>(json.GetErrorOffset())
            % rapidjson::GetParseError_En(json.GetParseError());
        SARUS_THROW_ERROR(message.str());
    }
    return json;
}

rapidjson::Document readJSON(const boost::filesystem::path& filename) {
    std::ifstream ifs(filename.string());
    auto json = parseJSONStream(ifs);
    if (json.HasParseError()) {
        auto message = boost::format(
            "Error parsing JSON file %s. Input data is not valid JSON\n"
            "Error(offset %u): %s")
            % filename
            % static_cast<unsigned>(json.GetErrorOffset())
            % rapidjson::GetParseError_En(json.GetParseError());
        SARUS_THROW_ERROR(message.str());
    }
    return json;
}

rapidjson::SchemaDocument readJSONSchema(const boost::filesystem::path& schemaFile) {
    class RemoteSchemaDocumentProvider : public rapidjson::IRemoteSchemaDocumentProvider {
    public:
        RemoteSchemaDocumentProvider(const boost::filesystem::path& schemasDir)
            : schemasDir{schemasDir}
        {}
        const rapidjson::SchemaDocument* GetRemoteDocument(const char* uri, rapidjson::SizeType length) override {
            auto filename = std::string(uri, length);
            auto schema = common::readJSON(schemasDir / filename);
            return new rapidjson::SchemaDocument(schema);
        }
    private:
        boost::filesystem::path schemasDir;
    };

    auto schemaJSON = common::readJSON(schemaFile);
    auto provider = RemoteSchemaDocumentProvider{ schemaFile.parent_path() };
    return rapidjson::SchemaDocument{ schemaJSON, nullptr, rapidjson::SizeType(0), &provider };
}

rapidjson::Document readAndValidateJSON(const boost::filesystem::path& jsonFile, const boost::filesystem::path& schemaFile) {
    auto schema = readJSONSchema(schemaFile);

    rapidjson::Document json;

    // Use a reader object to parse the JSON storing configuration settings
    try {
        std::ifstream configInputStream(jsonFile.string());
        rapidjson::IStreamWrapper configStreamWrapper(configInputStream);
        // Parse JSON from reader, validate the SAX events, and populate the configuration Document.
        rapidjson::SchemaValidatingReader<rapidjson::kParseDefaultFlags, rapidjson::IStreamWrapper, rapidjson::UTF8<> > reader(configStreamWrapper, schema);
        json.Populate(reader);

        // Check parsing outcome
        if (!reader.GetParseResult()) {
            // Not a valid JSON
            // When reader.GetParseResult().Code() == kParseErrorTermination,
            // it may be terminated by:
            // (1) the validator found that the JSON is invalid according to schema; or
            // (2) the input stream has I/O error.
            // Check the validation result
            if (!reader.IsValid()) {
                // Input JSON is invalid according to the schema
                // Output diagnostic information
                rapidjson::StringBuffer sb;
                reader.GetInvalidSchemaPointer().StringifyUriFragment(sb);
                auto message = boost::format("Invalid schema: %s\n") % sb.GetString();
                message = boost::format("%sInvalid keyword: %s\n") % message % reader.GetInvalidSchemaKeyword();
                sb.Clear();
                reader.GetInvalidDocumentPointer().StringifyUriFragment(sb);
                message = boost::format("%sInvalid document: %s\n") % message % sb.GetString();
                // Detailed violation report is available as a JSON value
                sb.Clear();
                rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);
                reader.GetError().Accept(w);
                message = boost::format("%sError report:\n%s") % message % sb.GetString();
                SARUS_THROW_ERROR(message.str());
            }
            else {
                auto message = boost::format("Error parsing JSON file: %s") % jsonFile;
                SARUS_THROW_ERROR(message.str());
            }
        }
    }
    catch(const common::Error&) {
        throw;
    }
    catch (const std::exception& e) {
        auto message = boost::format("Error reading JSON file %s") % jsonFile;
        SARUS_RETHROW_ERROR(e, message.str());
    }

    return json;
}

void writeJSON(const rapidjson::Value& json, const boost::filesystem::path& filename) {
    try {
        createFoldersIfNecessary(filename.parent_path());
        std::ofstream ofs(filename.string());
        if(!ofs) {
            auto message = boost::format("Failed to open std::ofstream for %s") % filename;
            SARUS_THROW_ERROR(message.str());
        }
        rapidjson::OStreamWrapper osw(ofs);
        rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
        writer.SetIndent(' ', 3);
        json.Accept(writer);
    }
    catch(const std::exception& e) {
        auto message = boost::format("Failed to write JSON to %s") % filename;
        SARUS_RETHROW_ERROR(e, message.str());
    }
}

std::string serializeJSON(const rapidjson::Value& json) {
    namespace rj = rapidjson;
    rj::StringBuffer buffer;
    rj::Writer<rj::StringBuffer> writer(buffer);
    json.Accept(writer);
    return buffer.GetString();
}

void logMessage(const boost::format& message, LogLevel level, std::ostream& out, std::ostream& err) {
    logMessage(message.str(), level, out, err);
}

void logMessage(const std::string& message, LogLevel level, std::ostream& out, std::ostream& err) {
    auto subsystemName = "CommonUtility";
    common::Logger::getInstance().log(message, subsystemName, level, out, err);
}

} // namespace
} // namespace
