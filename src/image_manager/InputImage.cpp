#include "InputImage.hpp"

#include <chrono>
#include <archive_entry.h> // libarchive

#include "common/Utility.hpp"


namespace sarus {
namespace image_manager {

InputImage::InputImage(const common::Config& config)
    : config{&config}
{}

boost::filesystem::path InputImage::makeTemporaryExpansionDirectory() const {
    auto tempExpansionDir = common::makeUniquePathWithRandomSuffix(config->directories.temp / "expansion-directory");
    common::createFoldersIfNecessary(tempExpansionDir);
    return tempExpansionDir;
}

void InputImage::expandLayers(  const std::vector<boost::filesystem::path>& layersPaths,
                                const boost::filesystem::path& expandDir) const {
    log(boost::format("expanding image layers"), common::logType::INFO);
    log(boost::format("> expanding image layers ..."), common::logType::GENERAL);

    auto timeStart = std::chrono::system_clock::now();

    auto cwd = boost::filesystem::current_path();
    common::changeDirectory(expandDir);

    std::vector<std::string> excludePattern = {"^dev/", "^/", "../"};
    const std::string sha256OfEmptyTarArchive = "sha256:a3ed95caeb02ffe68cdd9fd84406680ae93d633cb16422d00e8a7c22955b46d4";

    // expand layers (from parent to child)
    for (const auto archivePath : layersPaths) {
        // skip empty layer
        if(archivePath.filename().string() == (sha256OfEmptyTarArchive + ".tar") ) {
            continue;
        }

        if(!boost::filesystem::exists(archivePath) ) {
            SARUS_THROW_ERROR("Missing layer archive: " + archivePath.string());
        }

        // reserve whiteouts capacity
        std::vector<std::string> whiteouts;
        whiteouts.reserve(500);

        log(boost::format("> %-15.15s: %s") % "extracting" % archivePath, common::logType::GENERAL);

        // extract layer tarfile & get whiteouts list
        extractArchiveWithExcludePatterns(archivePath, excludePattern, whiteouts);

        // to make sure everything is writable by user
        boost::filesystem::recursive_directory_iterator it(expandDir), end;
        while( it!= end )
        {
            // skip symbolic link
            if ( !boost::filesystem::is_symlink( it->path() ) )
            {
                // add writable permis to owner
                boost::filesystem::permissions(it->path(), 
                    boost::filesystem::add_perms | boost::filesystem::owner_write);
            }
            ++it;
        }
        log(boost::format("Remove whiteouts."), common::logType::DEBUG);
        
        // remove whiteous files/dirs
        for (std::string &wh: whiteouts)
        {
            boost::filesystem::path whSymbol = expandDir / boost::filesystem::path(wh);
            boost::filesystem::path whTarget = expandDir / boost::filesystem::path(wh.replace(wh.find_first_of(".wh."), 4, ""));

            if( boost::filesystem::exists(whSymbol)) {
                if (!boost::filesystem::remove_all(whSymbol) ){
                    log(boost::format("Failed to remove whiteout: %s") % whSymbol, common::logType::ERROR);
                }
            }
            if( boost::filesystem::exists(whTarget)) {
                if (!boost::filesystem::remove_all(whTarget) ){
                    log(boost::format("Failed to remove whiteout: %s") % whTarget, common::logType::ERROR);
                }
            }
        }
        
    }
        
    // change directory to previous working path
    common::changeDirectory(cwd);

    log(boost::format("making expanded layers readable by the world"), common::logType::DEBUG);

    // add permision to be readable (& executable for dir)
    boost::filesystem::recursive_directory_iterator it(expandDir), end;
    while(it!= end) {
        // skip symbolic link
        if(!boost::filesystem::is_symlink( it->path())) {
            // add readable permission to all
            boost::filesystem::permissions(it->path(), 
                boost::filesystem::add_perms | boost::filesystem::owner_read);

            // add executable permission for dirs to all
            if (!boost::filesystem::is_directory(it->path())) {
                boost::filesystem::permissions(it->path(), 
                    boost::filesystem::add_perms | boost::filesystem::owner_exe );
            }
        }
        ++it;
    }

    auto timeEnd = std::chrono::system_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd - timeStart).count() / double(1000);
    log(boost::format("Elapsed time expansion: %s [s]") % timeElapsed, common::logType::INFO);

    log(boost::format("successfully expanded image layers"), common::logType::INFO);
}

// Extract the specified archive into the current working directory
void InputImage::extractArchive(const boost::filesystem::path& archivePath) const {
    auto excludePatterns = std::vector<std::string>{};
    auto whiteouts = std::vector<std::string>{}; // dummy/ignored out parameter
    extractArchiveWithExcludePatterns(archivePath, excludePatterns, whiteouts);
}

// Extract the specified archive into the current working directory, drop
// the archive's entries that match the specified exclude patterns and return
// a list of the Docker whiteout symbols found in the archive (out parameter).
void InputImage::extractArchiveWithExcludePatterns( const boost::filesystem::path& archivePath,
                                                    const std::vector<std::string> &excludePattern, 
                                                    std::vector<std::string> &whiteouts) const {
    log(boost::format("extracting archive %s") % archivePath, common::logType::DEBUG);

    /* Select which attributes we want to restore */
    int flags = ARCHIVE_EXTRACT_TIME
                | ARCHIVE_EXTRACT_PERM
                | ARCHIVE_EXTRACT_ACL
                | ARCHIVE_EXTRACT_FFLAGS;
    
    ::archive* arc = archive_read_new();
    archive_read_support_format_all(arc);
    archive_read_support_filter_all(arc);

    ::archive* ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, flags);
    archive_write_disk_set_standard_lookup(ext);
    
    // define pattern to exclude files
    ::archive* matchToExclude = archive_match_new();
    for(const auto &pattern: excludePattern) {
        if (archive_match_exclude_pattern(matchToExclude, pattern.c_str() ) != ARCHIVE_OK){
            auto message = boost::format("invalid libarchive exclude pattern: %s") % pattern;
            SARUS_THROW_ERROR(message.str());
        }
    }
    // define pattern to mark the whiteouts
    ::archive* matchToMarkWhiteouts = archive_match_new();
    if (archive_match_exclude_pattern(matchToMarkWhiteouts, "^.wh.*" ) != ARCHIVE_OK) {
        auto message = boost::format("invalid libarchive exclude pattern: ^.wh.*");
        SARUS_THROW_ERROR(message.str());
    }
    
    //  open archive
    if ( archive_read_open_filename(arc, archivePath.string().c_str(), 10240) != ARCHIVE_OK ) {
        auto message = boost::format("failed to open archive %s") % archivePath;
        SARUS_THROW_ERROR(message.str());
    }
    
    // for all archive entries
    while(true)
    {
        // read entry header
        ::archive_entry *entry;
        int r = archive_read_next_header(arc, &entry);
        if (r == ARCHIVE_EOF) {
            break;
        }
        else if (r < ARCHIVE_WARN) {
            auto message = boost::format("archive %s: error while reading header of entry %s (%s)")
                        % archivePath % archive_entry_pathname(entry) % archive_error_string(arc);
            SARUS_THROW_ERROR(message.str());
        }
        else if (r < ARCHIVE_OK) {
            // appearently, even if something goes wrong while reading the entry's header,
            // it might still be possible to write / copy data of the entry. So let's just
            // issue an INFO log message at this stage. If a failure happens later while
            // copying the data, then we will issue an error.
            log(boost::format("archive: error while reading header of entry %s (%s)")
                % archive_entry_pathname(entry) % archive_error_string(arc),
                common::logType::INFO);
        }

        log(boost::format("archive: processing entry %s") % archive_entry_pathname(entry), common::logType::DEBUG);

        // if entry maches excluded pattern, memorize entry and skip extracting
        if ( archive_match_excluded(matchToExclude, entry) ) {
            log(boost::format("archive: skipping (excluded) entry"), common::logType::DEBUG);
            continue;
        }
    
        // if entry maches whiteouts pattern, memorize entry
        if ( archive_match_excluded(matchToMarkWhiteouts, entry) ) {
            log(boost::format("archive: entry is whiteout"), common::logType::DEBUG);
            whiteouts.push_back( archive_entry_pathname(entry) );
        }
        
        // write entry
        log(boost::format("archive: writing entry"), common::logType::DEBUG);
        r = archive_write_header(ext, entry);
        if (r < ARCHIVE_OK) {
            auto message = boost::format("archive %s: error while writing header of entry %s (%s)")
                % archivePath % archive_entry_pathname(entry) % archive_error_string(arc);
            SARUS_THROW_ERROR(message.str());
        }
        else if (archive_entry_size(entry) > 0) {
            log(boost::format("archive: copying data of entry"), common::logType::DEBUG);
            copyDataOfArchiveEntry(archivePath, arc, ext, entry);
        }
    
        r = archive_write_finish_entry(ext);
        if (r < ARCHIVE_WARN) {
            auto message = boost::format("archive %s: error while finishing to write entry %s (%s)")
                        % archivePath % archive_entry_pathname(entry) % archive_error_string(arc);
            SARUS_THROW_ERROR(message.str());
        }
        else if(r < ARCHIVE_OK) {
            log(   boost::format("archive %s: error while finishing to write entry %s (%s)")
                        % archivePath % archive_entry_pathname(entry) % archive_error_string(arc),
                        common::logType::INFO);
        }
    }

    archive_match_free(matchToExclude);
    archive_match_free(matchToMarkWhiteouts);
    archive_read_close(arc);
    archive_read_free(arc);
    archive_write_close(ext);
    archive_write_free(ext);

    log(boost::format("successfully extracted archive %s") % archivePath, common::logType::DEBUG);
}

void InputImage::copyDataOfArchiveEntry(const boost::filesystem::path& archivePath,
                                        ::archive* in,
                                        ::archive* out,
                                        ::archive_entry *entry) const {
    int r;
    const void *buff;
    size_t size;
    la_int64_t offset;

    while(true) {
        r = archive_read_data_block(in, &buff, &size, &offset);
        if(r == ARCHIVE_EOF) {
            return;
        }
        else if(r < ARCHIVE_OK) {
            break;
        }
    
        r = archive_write_data_block(out, buff, size, offset);
        if (r < ARCHIVE_OK) {
            break;
        }
    }

    if (r < ARCHIVE_WARN) {
        auto message = boost::format("Failed to copy data from archive %s. Error while copying entry %s: %s")
            % archivePath % archive_entry_pathname(entry) % archive_error_string(in);
        SARUS_THROW_ERROR(message.str());
    }
    else if (r < ARCHIVE_OK) {
        auto message = boost::format("Failed to copy data from archive %s. Error while copying entry %s: %s")
            % archivePath % archive_entry_pathname(entry) % archive_error_string(in);
        log(message, common::logType::INFO);
    }
}

void InputImage::log(const boost::format &message, common::logType level) const {
    log(message.str(), level);
}

void InputImage::log(const std::string& message, common::logType level) const {
    common::Logger::getInstance().log(message, "InputImage", level);
}

}} // namespace
