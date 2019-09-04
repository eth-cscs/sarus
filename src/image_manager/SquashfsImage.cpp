/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "SquashfsImage.hpp"

#include <chrono>
#include <boost/format.hpp>

#include "common/Utility.hpp"
#include "common/Logger.hpp"
#include "common/PathRAII.hpp"


namespace sarus {
namespace image_manager {

SquashfsImage::SquashfsImage(   const common::Config& config,
                                const boost::filesystem::path& expandedImage,
                                const boost::filesystem::path& pathOfImage)
    : pathOfImage{pathOfImage}
{
    auto pathTemp = common::PathRAII{common::makeUniquePathWithRandomSuffix(pathOfImage)};
    common::createFoldersIfNecessary(pathTemp.getPath().parent_path());

    log(boost::format("> make squashfs image: %s") % pathOfImage, common::logType::GENERAL);
    log(boost::format("creating squashfs image %s from expanded image %s") % pathOfImage % expandedImage,
        common::logType::INFO);

    auto start = std::chrono::system_clock::now();

    
    boost::filesystem::path mksquashfsPath(config.json["mksquashfsPath"].GetString());
    auto mksquashfsCommand = mksquashfsPath.string() + " " + expandedImage.string() + " " + pathTemp.getPath().string();
    common::executeCommand(mksquashfsCommand);
    boost::filesystem::rename(pathTemp.getPath(), pathOfImage); // atomically create/replace squashfs file
    pathTemp.release();

    auto end = std::chrono::system_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / double(1000);
    log(boost::format("Elapsed time mksquashfs: %s [s]") % elapsedTime, common::logType::INFO);

    log(boost::format("successfully created squashfs file"), common::logType::INFO);
}

boost::filesystem::path SquashfsImage::getPathOfImage() const {
    return pathOfImage;
}

void SquashfsImage::log(const boost::format &message, common::logType level) const {
    common::Logger::getInstance().log(message, "SquashfsImage", level);
}

}} // namespace
