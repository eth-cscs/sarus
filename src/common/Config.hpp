#ifndef sarus_common_Config_hpp
#define sarus_common_Config_hpp

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "common/CLIArguments.hpp"
#include "common/ImageID.hpp"
#include "common/Logger.hpp"
#include "runtime/Mount.hpp"


namespace sarus {
namespace common {

struct Config {
    struct BuildTime {
        BuildTime();
        std::string version;
        boost::filesystem::path prefixDir;
        boost::filesystem::path configFile;
        boost::filesystem::path configSchemaFile;
        boost::filesystem::path localRepositoryFolder = ".sarus";
        boost::filesystem::path openSshArchive;
        bool areRuntimeSecurityChecksEnabled;
    };

    struct Directories {
        void initialize(bool useCentralizedRepository, const common::Config& config);
        boost::filesystem::path repository;
        boost::filesystem::path cache;
        boost::filesystem::path temp;
        std::string tempFromCLI;
        boost::filesystem::path images;
    };

    struct JSON {
        JSON();
        void initialize(const boost::filesystem::path& configFilename, const boost::filesystem::path& schemaFilename);
        rapidjson::Document& get() { return *(this->file); }
        const rapidjson::Document& get() const { return *(this->file); }
    private:
        std::shared_ptr<rapidjson::Document> file;
    };

    struct UserIdentity {
        UserIdentity();
        uid_t uid;
        gid_t gid;
        std::vector<gid_t> supplementaryGids;
    };

    struct Authentication {
        bool isAuthenticationNeeded = false;
        std::string username;
        std::string password;
    };

    struct CommandRun {
        std::unordered_map<std::string, std::string> hostEnvironment;
        std::vector<std::string> userMounts;
        std::vector<std::shared_ptr<runtime::Mount>> mounts;
        boost::optional<CLIArguments> entrypoint;
        CLIArguments execArgs;
        bool allocatePseudoTTY = false;
        bool useMPI = false;
        bool enableSSH = false;
    };

    boost::filesystem::path getImageFile() const;
    boost::filesystem::path getMetadataFileOfImage() const;

    BuildTime buildTime;
    common::ImageID imageID;
    Directories directories;
    JSON json;
    UserIdentity userIdentity;
    Authentication authentication;
    CommandRun commandRun;

    boost::filesystem::path archivePath; // for CommandLoad

    bool useCentralizedRepository = false;

    std::chrono::high_resolution_clock::time_point program_start; // for time measurement
};

}
}

#endif
