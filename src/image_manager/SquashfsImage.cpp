/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "SquashfsImage.hpp"

#include <chrono>
#include <boost/format.hpp>

#include "libsarus/Utility.hpp"
#include "libsarus/Logger.hpp"
#include "libsarus/PathRAII.hpp"


namespace sarus {
namespace image_manager {

libsarus::CLIArguments SquashfsImage::generateMksquashfsArgs(const common::Config& config,
                                                           const boost::filesystem::path& sourcePath,
                                                           const boost::filesystem::path& destinationPath) {
    auto mksquashfsPath = boost::filesystem::path(config.json["mksquashfsPath"].GetString());
    auto args = libsarus::CLIArguments{mksquashfsPath.string(), sourcePath.string(), destinationPath.string()};
    if (const rapidjson::Value* configOpts = rapidjson::Pointer("/mksquashfsOptions").Get(config.json)) {
        args.push_back(configOpts->GetString());
    }
    return args;
}

SquashfsImage::SquashfsImage(const common::Config& config,
                             const boost::filesystem::path& unpackedImage,
                             const boost::filesystem::path& pathOfImage)
    : pathOfImage{pathOfImage}
{
    auto pathTemp = libsarus::PathRAII{libsarus::filesystem::makeUniquePathWithRandomSuffix(pathOfImage)};
    libsarus::filesystem::createFoldersIfNecessary(pathTemp.getPath().parent_path());

    log(boost::format("> making squashfs image: %s") % pathOfImage, libsarus::LogLevel::GENERAL);
    log(boost::format("creating squashfs image %s from unpacked image %s") % pathOfImage % unpackedImage,
        libsarus::LogLevel::INFO);

    auto start = std::chrono::system_clock::now();
    
    auto args = generateMksquashfsArgs(config, unpackedImage, pathTemp.getPath());
    auto mksquashfsOutput = libsarus::process::executeCommand(args.string());
    log(boost::format("mksquashfs output:\n%s") % mksquashfsOutput, libsarus::LogLevel::DEBUG);

    boost::filesystem::rename(pathTemp.getPath(), pathOfImage); // atomically create/replace squashfs file
    pathTemp.release();

    auto end = std::chrono::system_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / double(1000);
    log(boost::format("Elapsed time on mksquashfs: %s [s]") % elapsedTime, libsarus::LogLevel::INFO);

    log(boost::format("successfully created squashfs file"), libsarus::LogLevel::INFO);
}

boost::filesystem::path SquashfsImage::getPathOfImage() const {
    return pathOfImage;
}

void SquashfsImage::log(const boost::format &message, libsarus::LogLevel level) const {
    libsarus::Logger::getInstance().log(message, "SquashfsImage", level);
}

}} // namespace
