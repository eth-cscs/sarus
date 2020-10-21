/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "InputImage.hpp"

#include <chrono>
#include <archive_entry.h> // libarchive

#include "common/Utility.hpp"


namespace sarus {
namespace image_manager {

InputImage::InputImage(std::shared_ptr<const common::Config> config)
    : config{std::move(config)}
{}

boost::filesystem::path InputImage::makeTemporaryExpansionDirectory() const {
    auto tempExpansionDir = common::makeUniquePathWithRandomSuffix(config->directories.temp / "expansion-directory");
    try {
        common::createFoldersIfNecessary(tempExpansionDir);
    }
    catch(common::Error& e) {
        auto message = boost::format("Invalid temporary directory %s") % config->directories.temp;
        log(message, common::LogLevel::GENERAL, std::cerr);
        SARUS_RETHROW_ERROR(e, message.str(), common::LogLevel::INFO);
    }
    return tempExpansionDir;
}

void InputImage::expandLayers(  const std::vector<boost::filesystem::path>& layersPaths,
                                const boost::filesystem::path& expandDir) const {
    log(boost::format("expanding image layers"), common::LogLevel::INFO);
    log(boost::format("> expanding image layers ..."), common::LogLevel::GENERAL);

    auto timeStart = std::chrono::system_clock::now();

    std::vector<std::string> excludePattern = {"^dev/", "^/", "../", ".wh.*"};
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

        log(boost::format("> %-15.15s: %s") % "extracting" % archivePath, common::LogLevel::GENERAL);

        // extract layer tarfile & get whiteouts list
        auto whiteouts = readWhiteoutsInLayer(archivePath);
        applyWhiteouts(whiteouts, expandDir);

        extractArchiveWithExcludePatterns(archivePath, excludePattern, expandDir);

        // change permissions for this user (+rw for files, +rwx for directories)
        for(auto it = boost::filesystem::recursive_directory_iterator{expandDir};
            it != boost::filesystem::recursive_directory_iterator{};
            ++it) {
            if(boost::filesystem::is_symlink(it->path())) {
                continue; // skip symbolik links
            }

            boost::filesystem::permissions(it->path(), boost::filesystem::add_perms
                                                    | boost::filesystem::owner_read
                                                    | boost::filesystem::owner_write);

            if(boost::filesystem::is_directory(it->path())) {
                boost::filesystem::permissions(it->path(),  boost::filesystem::add_perms | boost::filesystem::owner_exe);
            }
        }
    }

    log(boost::format("making expanded layers readable by the world"), common::LogLevel::DEBUG);

    auto timeEnd = std::chrono::system_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd - timeStart).count() / double(1000);
    log(boost::format("Elapsed time expansion: %s [s]") % timeElapsed, common::LogLevel::INFO);

    log(boost::format("successfully expanded image layers"), common::LogLevel::INFO);
}

// Extract the specified archive into the current working directory
void InputImage::extractArchive(const boost::filesystem::path& archivePath,
                                const boost::filesystem::path& expandDir) const {
    auto excludePatterns = std::vector<std::string>{};
    extractArchiveWithExcludePatterns(archivePath, excludePatterns, expandDir);
}

// Extract the specified archive into the specified expand directory and drop
// the archive's entries that match the specified exclude patterns.
void InputImage::extractArchiveWithExcludePatterns( const boost::filesystem::path& archivePath,
                                                    const std::vector<std::string> &excludePattern,
                                                    const boost::filesystem::path& expandDir) const {
    log(boost::format("extracting archive %s") % archivePath, common::LogLevel::DEBUG);

    auto cwd = boost::filesystem::current_path();
    common::changeDirectory(expandDir);

    /* Select which attributes we want to restore */
    int flags = ARCHIVE_EXTRACT_TIME
                | ARCHIVE_EXTRACT_PERM
                | ARCHIVE_EXTRACT_ACL
                | ARCHIVE_EXTRACT_FFLAGS
                | ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS
                | ARCHIVE_EXTRACT_SECURE_NODOTDOT
                | ARCHIVE_EXTRACT_SECURE_SYMLINKS;

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
                common::LogLevel::INFO);
        }

        auto archiveEntryPath = boost::filesystem::path(archive_entry_pathname(entry));
        log(boost::format("archive: processing entry %s") % archiveEntryPath, common::LogLevel::DEBUG);

        // if entry maches excluded pattern, memorize entry and skip extracting
        if ( archive_match_excluded(matchToExclude, entry) ) {
            log(boost::format("archive: skipping (excluded) entry"), common::LogLevel::DEBUG);
            continue;
        }
        
        // Clobber file in extraction path to avoid errors, unless the file in
        // the extraction path and the entry from the archive are both directories
        if ( !(boost::filesystem::is_directory(expandDir / archiveEntryPath) && archive_entry_filetype(entry) == AE_IFDIR) ) {
            boost::filesystem::remove_all(expandDir / archiveEntryPath);
        }


        // write entry
        log(boost::format("archive: writing entry"), common::LogLevel::DEBUG);
        r = archive_write_header(ext, entry);
        if (r < ARCHIVE_OK) {
            auto message = boost::format("archive %s: error while writing header of entry %s (%s)")
                % archivePath % archiveEntryPath % archive_error_string(ext);
            SARUS_THROW_ERROR(message.str());
        }
        else if (archive_entry_size(entry) > 0) {
            log(boost::format("archive: copying data of entry"), common::LogLevel::DEBUG);
            copyDataOfArchiveEntry(archivePath, arc, ext, entry);
        }
    
        r = archive_write_finish_entry(ext);
        if (r < ARCHIVE_WARN) {
            auto message = boost::format("archive %s: error while finishing to write entry %s (%s)")
                        % archivePath % archiveEntryPath % archive_error_string(ext);
            SARUS_THROW_ERROR(message.str());
        }
        else if(r < ARCHIVE_OK) {
            log(   boost::format("archive %s: error while finishing to write entry %s (%s)")
                        % archivePath % archiveEntryPath % archive_error_string(ext),
                        common::LogLevel::INFO);
        }
    }

    archive_match_free(matchToExclude);
    archive_read_close(arc);
    archive_read_free(arc);
    archive_write_close(ext);
    archive_write_free(ext);

    common::changeDirectory(cwd); // move back to original working dir

    log(boost::format("successfully extracted archive %s") % archivePath, common::LogLevel::DEBUG);
}

std::vector<boost::filesystem::path> InputImage::readWhiteoutsInLayer(const boost::filesystem::path& layerArchive) const {
    log(boost::format("reading whiteout files in layer archive %s") % layerArchive, common::LogLevel::DEBUG);

    auto whiteouts = std::vector<boost::filesystem::path>{};

    ::archive* arc = archive_read_new();
    archive_read_support_format_all(arc);
    archive_read_support_filter_all(arc);

    ::archive* match = archive_match_new();
    if(archive_match_exclude_pattern(match, ".wh.*" ) != ARCHIVE_OK) {
        auto message = boost::format("invalid libarchive exclude pattern");
        SARUS_THROW_ERROR(message.str());
    }

    if (archive_read_open_filename(arc, layerArchive.string().c_str(), 10240) != ARCHIVE_OK ) {
        auto message = boost::format("failed to open archive %s") % layerArchive;
        SARUS_THROW_ERROR(message.str());
    }
    
    // for each archive entry
    while(true) {
        ::archive_entry *entry;
        int r = archive_read_next_header(arc, &entry);
        if (r == ARCHIVE_EOF) {
            break;
        }
        else if (r < ARCHIVE_WARN) {
            auto message = boost::format("archive %s: error while reading header of entry %s (%s)")
                        % layerArchive % archive_entry_pathname(entry) % archive_error_string(arc);
            SARUS_THROW_ERROR(message.str());
        }
        else if (r < ARCHIVE_OK) {
            // appearently, even if something goes wrong while reading the entry's header,
            // it might still be possible to write / copy data of the entry. So let's just
            // issue an INFO log message at this stage. If a failure happens later while
            // copying the data, then we will issue an error.
            log(boost::format("archive: error while reading header of entry %s (%s)")
                % archive_entry_pathname(entry) % archive_error_string(arc),
                common::LogLevel::INFO);
        }

        log(boost::format("archive: processing entry %s") % archive_entry_pathname(entry), common::LogLevel::DEBUG);
    
        if(archive_match_excluded(match, entry)) {
            log(boost::format("archive: entry is whiteout"), common::LogLevel::DEBUG);
            whiteouts.push_back(archive_entry_pathname(entry));
        }
    }

    archive_match_free(match);
    archive_read_close(arc);
    archive_read_free(arc);

    log(boost::format("successfully read whiteout files"), common::LogLevel::DEBUG);

    return whiteouts;
}

void InputImage::applyWhiteouts(const std::vector<boost::filesystem::path>& whiteouts,
                                const boost::filesystem::path& expandDir) const {
    log(boost::format("Applying whiteouts"), common::LogLevel::INFO);

    for(const auto& whiteout : whiteouts) {
        auto whiteoutFile = expandDir / whiteout;

        // opaque whiteouts:
        // remove all the contents of the whiteout's parent directory
        bool isOpaqueWhiteout = whiteoutFile.filename() == ".wh..wh..opq";
        if(isOpaqueWhiteout) {
            auto target = whiteoutFile.parent_path();
            log(boost::format("Applying opaque whiteout to target directory %s") % target, common::LogLevel::DEBUG);
            if(!boost::filesystem::is_directory(target)) {
                log(boost::format("Skipping whiteout because target %s is not a directory") % target,
                    common::LogLevel::DEBUG);
                continue;
            }
            for(boost::filesystem::directory_iterator end, it(target); it != end; ++it) {
                if(!boost::filesystem::remove_all(it->path())) {
                    log(boost::format("Failed to whiteout %s") % it->path(), common::LogLevel::ERROR);
                }
            }
        }
        // regular whiteout:
        // remove the single file or folder that corresponds to the whiteout
        else {
            auto target = whiteoutFile.parent_path();
            target /= whiteoutFile.filename().c_str() + 4; // remove leading ".wh." characters in filename
            log(boost::format("Applying regular whiteout to %s") % target, common::LogLevel::DEBUG);
            if(!boost::filesystem::remove_all(target)) {
                log(boost::format("Failed to whiteout %s") % target, common::LogLevel::ERROR);
            }
        }
    }

    log(boost::format("Successfully applied whiteouts"), common::LogLevel::INFO);
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
        log(message, common::LogLevel::INFO);
    }
}

void InputImage::log(   const boost::format &message, common::LogLevel level,
                        std::ostream& outStream, std::ostream& errStream) const {
    log(message.str(), level, outStream, errStream);
}

void InputImage::log(   const std::string& message, common::LogLevel level,
                        std::ostream& outStream, std::ostream& errStream) const {
    common::Logger::getInstance().log(message, "InputImage", level, outStream, errStream);
}

}} // namespace
