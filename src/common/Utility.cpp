/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "Utility.hpp"

#include <fstream>
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
#include <pwd.h>
#include <grp.h>
#include <limits.h>

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

std::tuple<std::string, std::string> parseEnvironmentVariable(const std::string& variable) {
    auto keyEnd = std::find(variable.cbegin(), variable.cend(), '=');
    if(keyEnd == variable.cend()) {
        auto message = boost::format("Failed to parse environment variable \"%s\". Expected symbol '='.")
            % variable;
        SARUS_THROW_ERROR(message.str());
    }

    auto key = std::string(variable.cbegin(), keyEnd);
    auto value = std::string(keyEnd+1, variable.cend());

    return std::tuple<std::string, std::string>{key, value};
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

void setEnvironmentVariable(const std::string& variable) {
    std::string key;
    std::string value;
    std::tie(key, value) = parseEnvironmentVariable(variable);
    int overwrite = 1;

    if(setenv(key.c_str(), value.c_str(), overwrite) != 0) {
        auto message = boost::format("Failed to setenv(%s, %s, %d): %s")
            % key % value % overwrite % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    logMessage(boost::format("Put environment variable %s") % variable, common::LogLevel::DEBUG);
}

std::string removeWhitespaces(const std::string& s) {
    auto result = s;
    auto newEnd = std::remove_if(result.begin(), result.end(), iswspace);
    result.erase(newEnd, result.end());
    return result;
}

std::string replaceString(std::string &buf, const std::string& from, const std::string& to)
{
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

void switchToUnprivilegedUser(const common::UserIdentity& identity) {
    logMessage(boost::format{"Switching to uprivileged user (uid=%d gid=%d)"}
               % identity.uid % identity.gid,
               LogLevel::DEBUG);

    if (setgroups(identity.supplementaryGids.size(), identity.supplementaryGids.data()) != 0) {
        SARUS_THROW_ERROR("Failed to assume end-user auxiliary gids");
    }
    if (setegid(identity.gid) != 0) {
        SARUS_THROW_ERROR("Failed to assume end-user gid");
    }
    if (seteuid(identity.uid) != 0) {
        SARUS_THROW_ERROR("Failed to assume end-user uid");
    }

    logMessage("Successfully switched to uprivileged user", LogLevel::DEBUG);
}

void switchToPrivilegedUser(const common::UserIdentity& identity) {
    logMessage(boost::format{"Switching to privileged user (uid=%d gid=%d)"}
               % identity.uid % identity.gid,
               LogLevel::DEBUG);

    if (seteuid(identity.uid) != 0) {
        SARUS_THROW_ERROR("Failed to re-assume original user effective uid");
    }
    if (setegid(identity.uid) != 0) {
        SARUS_THROW_ERROR("Failed to re-assume original user effective gid");
    }
    if (setgroups(identity.supplementaryGids.size(), identity.supplementaryGids.data()) != 0) {
        SARUS_THROW_ERROR("Failed to re-assume original user auxiliary gids");
    }

    logMessage("Successfully switched to privileged user", LogLevel::DEBUG);
}

std::string executeCommand(const std::string& command) {
    auto commandWithRedirection = command + " 2>&1"; // stderr-to-stdout redirection necessary because popen only reads stdout

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
                 const boost::optional<std::function<void()>>& preExecActions) {
    logMessage(boost::format("Executing %s") % args, common::LogLevel::DEBUG);

    // fork and execute
    auto pid = fork();
    if(pid == -1) {
        auto message = boost::format("Failed to fork to execute subprocess %s: %s")
            % args % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }

    bool isChild = pid == 0;
    if(isChild) {
        if(preExecActions) {
            (*preExecActions)();
        }
        execvp(args.argv()[0], args.argv());
        auto message = boost::format("Failed to execvp subprocess %s: %s") % args % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    else {
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
    if(stat(filename.string().c_str(), &st) != 0) {
        auto message = boost::format("Failed to retrieve size of file %s. Stat failed: %s")
            % filename % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    return st.st_size;
}

std::tuple<uid_t, gid_t> getOwner(const boost::filesystem::path& path) {
    struct stat sb;
    if(stat(path.c_str(), &sb) != 0) {
        auto message = boost::format("Failed to stat %s: %s") % path % strerror(errno);
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
                // the creation might have failed because another process cuncurrently
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
    // NOTE: Broken symlinks will NOT be recognized as existing and hence will be overriden.
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

static bool isSymlink(const boost::filesystem::path& path) {
    struct stat sb;
    if (lstat(path.string().c_str(), &sb) != 0) {
        return false;
    }
    return (sb.st_mode & S_IFMT) == S_IFLNK;
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
                                                      const std::string& pairSeparators,
                                                      const std::string& keyValueSeparators) {
    // check for empty input
    if(input.empty()) {
        return std::unordered_map<std::string, std::string>{};
    }

    auto map = std::unordered_map<std::string, std::string>{};

    auto pairs = std::vector<std::string>{};
    boost::split(pairs, input, boost::is_any_of(pairSeparators));

    for(const auto& pair : pairs) {
        auto tokens = std::vector<std::string>{};
        boost::split(tokens, pair, boost::is_any_of(keyValueSeparators));
        const auto& key = tokens[0];
        auto value = tokens.size() > 1 ? std::move(tokens[1]) : "";

        // check for "too many" tokens
        if(tokens.size() > 2) {
            auto message = boost::format("Error: found invalid key-value pair '%s' in '%s' (too many values).")
                % pair % input;
            logMessage(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO)
        }

        // check for empty key
        if(key.empty()) {
            auto message = boost::format("Error: found empty key in '%s'. Expected a list of key-value pairs.")
                % input;
            logMessage(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO)
        }

        // check for duplicated key
        if(map.find(key) != map.cend()) {
            auto message = boost::format("Error: found duplicated key '%s' in '%s'. Expected a list of unique key-value pairs.")
                % key % input;
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
    std::getline(stream, line); // drop first line of output (header)
    while(std::getline(stream, line)) {
        auto pos = line.rfind(" => ");
        auto library = line.substr(pos + 4);
        libraries.push_back(library);
    }

    return libraries;
}

bool isSharedLib(const boost::filesystem::path& file) {
    auto filename = file.filename().string();

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

std::tuple<unsigned int, unsigned int> parseLibcVersion(const boost::filesystem::path& lib) {
    boost::cmatch matches;
    boost::regex re("^(.*/)*libc-(\\d+)\\.(\\d+)?\\.so$");
    if (boost::regex_match(lib.c_str(), matches, re)) {
        return std::tuple<unsigned int, unsigned int>{
            std::stoi(matches[2]),
            std::stoi(matches[3])
        };
    }
    else {
        auto message = boost::format("Failed to parse libc ABI version from %s.") % lib;
        SARUS_THROW_ERROR(message.str());
    }
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

std::string parseCpusAllowedList(const std::string& procStatus) {
    logMessage("Parsing Cpus_allowed_list in contents of /proc/<pid>/status", LogLevel::DEBUG);

    auto lines = std::vector<std::string>{};
    boost::split(lines, procStatus, boost::is_any_of("\n"));

    boost::regex re("^\\s*Cpus_allowed_list:\\s*(\\S+)\\s*$");
    boost::cmatch matches;

    for(const auto& line : lines) {
        if (boost::regex_match(line.c_str(), matches, re)) {
            auto list = std::move(matches[1]);
            auto message = boost::format("Successfully parsed CpusAllosedList=%s"
                                        " in contents of /proc/<pid>/status") % list;
            logMessage(message.str(), LogLevel::DEBUG);
            return list;
        }
    }

    SARUS_THROW_ERROR("Failed to parse Cpus_allowed_list in contents of /proc/<pid>/status");
}

std::string readFile(const boost::filesystem::path& path) {
    std::ifstream ifs(path.string());
    auto s = std::string(   std::istreambuf_iterator<char>(ifs),
                            std::istreambuf_iterator<char>());
    return s;
}

rapidjson::Document readJSON(const boost::filesystem::path& filename) {
    auto json = rapidjson::Document{};

    try {
        std::ifstream ifs(filename.string());
        rapidjson::IStreamWrapper isw(ifs);
        json.ParseStream(isw);
    }
    catch (const std::exception& e) {
        auto message = boost::format("Error reading JSON from %s") % filename;
        SARUS_RETHROW_ERROR(e, message.str());
    }

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
        rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
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

rapidjson::Document convertCppRestJsonToRapidJson(web::json::value& cppRest) {
    try {
        std::stringstream is(cppRest.serialize());
        rapidjson::IStreamWrapper isw(is);
        rapidjson::Document doc;
        doc.ParseStream(isw);
        return doc;
    }
    catch(const std::exception& e) {
        auto message = boost::format("Failed to parse rapidjson object from JSON string %s")
            % cppRest.serialize();
        SARUS_RETHROW_ERROR(e, message.str());
    }
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
