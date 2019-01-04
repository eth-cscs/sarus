#include "LoadedImage.hpp"

#include "common/Utility.hpp"


namespace sarus {
namespace image_manager {

LoadedImage::LoadedImage(   std::shared_ptr<const common::Config> config,
                            const boost::filesystem::path& imageArchive)
    : InputImage{std::move(config)}
    , imageArchive{imageArchive}
{}

std::tuple<common::PathRAII, common::ImageMetadata, std::string> LoadedImage::expand() const {
    log(boost::format("expanding loaded image from archive %s") % imageArchive, common::logType::INFO);

    auto initialWorkingDir = boost::filesystem::current_path();
    auto tempArchiveDir = common::PathRAII{makeTemporaryExpansionDirectory()};
    auto expansionDir = common::PathRAII{makeTemporaryExpansionDirectory()};

    common::changeDirectory(tempArchiveDir.getPath());

    try {
        extractArchive(imageArchive);
    }
    catch(common::Error& e) {
        auto message = boost::format("failed to extract archive %s") % imageArchive;
        SARUS_RETHROW_ERROR(e, message.str());
    }
    common::changeDirectory( initialWorkingDir );

    // read manifest.json to construct metadata
    boost::filesystem::path manifestFilePath( tempArchiveDir.getPath() / "manifest.json" );
    auto loadedManifest = common::readJSON(manifestFilePath);
    log("manifest.json: " + common::serializeJSON(loadedManifest), common::logType::DEBUG);

    // if archive contains more than 1 container manifest or empty, throw error
    if (loadedManifest.Size() != 1) {
        auto message = boost::format("expected archive %s to contain exactly one manifest, but found %d")
            % imageArchive % loadedManifest.Size();
        SARUS_THROW_ERROR(message.str());
    }

    // read manifest
    auto& manifest = loadedManifest.GetArray()[0];
    auto& layers = manifest["Layers"];
    auto& RepoTags = manifest["RepoTags"];

    // parse config json
    auto configFile = tempArchiveDir.getPath() / manifest["Config"].GetString();
    auto imageConfig = common::readJSON(configFile);
    if (!imageConfig.HasMember("config")) {
        auto message = boost::format(   "Image configuration file %s is malformed: "
                                        "no \"config\" field detected") % configFile;
        SARUS_THROW_ERROR(message.str());
    }
    auto metadata = common::ImageMetadata(imageConfig["config"]);

    log("Config: " + common::serializeJSON(imageConfig), common::logType::DEBUG);
    log("layers: %s" + common::serializeJSON(layers), common::logType::DEBUG);
    log("Repotags: %s" + common::serializeJSON(RepoTags), common::logType::DEBUG);

    // create list of layer archive paths
    std::vector<boost::filesystem::path> layerArchives;
    for(const auto& layer: layers.GetArray()) {
        std::string layerArchive = layer.GetString();
        boost::filesystem::path layerArchivePath(tempArchiveDir.getPath() / layerArchive);
        layerArchives.push_back(layerArchivePath);
    }

    expandLayers(layerArchives, expansionDir.getPath());

    log(boost::format("successfully expanded loaded image from archive %s") % imageArchive, common::logType::INFO);

    auto digest = configFile.stem().string();
    return std::tuple<common::PathRAII, common::ImageMetadata, std::string>{
        std::move(expansionDir), std::move(metadata), digest
    };
}

}} // namespace
