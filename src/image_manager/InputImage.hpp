/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_image_manger_InputImage_hpp
#define sarus_image_manger_InputImage_hpp

#include <memory>
#include <tuple>
#include <vector>
#include <archive.h> // libarchive
#include <boost/filesystem.hpp>

#include "common/Config.hpp"
#include "common/PathRAII.hpp"
#include "common/ImageMetadata.hpp"

namespace sarus {
namespace image_manager {

/**
 * This class represents an input image prior extraction.
 */
class InputImage {
public:
    InputImage(std::shared_ptr<const common::Config> config);
    virtual std::tuple<common::PathRAII, common::ImageMetadata, std::string> expand() const = 0;

protected:
    boost::filesystem::path makeTemporaryExpansionDirectory() const;
    void expandLayers(  const std::vector<boost::filesystem::path>& layersPaths,
                        const boost::filesystem::path& expandDir) const;
    void extractArchive(const boost::filesystem::path& archivePath,
                        const boost::filesystem::path& expandDir) const;
    void extractArchiveWithExcludePatterns( const boost::filesystem::path& archivePath,
                                            const std::vector<std::string> &excludePattern,
                                            const boost::filesystem::path& expandDir) const;
    std::vector<boost::filesystem::path> readWhiteoutsInLayer(const boost::filesystem::path& layerArchive) const;
    void applyWhiteouts(const std::vector<boost::filesystem::path>& whiteouts,
                        const boost::filesystem::path& expandDir) const;
    void copyDataOfArchiveEntry(const boost::filesystem::path& archivePath,
                                ::archive* in,
                                ::archive* out,
                                ::archive_entry *entry) const;
    void log(   const boost::format &message, common::LogLevel,
                std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr) const;
    void log(   const std::string& message, common::LogLevel,
                std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr) const;

protected:
    std::shared_ptr<const common::Config> config;
};

}
}

#endif
