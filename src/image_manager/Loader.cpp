/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "image_manager/Loader.hpp"

#include <iostream>
#include <string>
#include <memory>
#include <chrono>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "common/Utility.hpp"
#include "common/CLIArguments.hpp"
#include "image_manager/Utility.hpp"

namespace sarus {
namespace image_manager {

Loader::Loader(std::shared_ptr<const common::Config> config)
    : config{std::move(config)}
{}

OCIImage Loader::load(const boost::filesystem::path& imageArchive) {
    std::chrono::system_clock::time_point start, end;
    double elapsed;

    // TODO when supporting digests in ImageReference class, issue error if config->ImageReference
    // (which constitutes the destination reference) contains a digest.
    // A digest in the context of a Sarus pull is the immutable identifier for an image when stored in a remote registry.
    // Thus, a digest makes no sense for a loaded image.

    auto skopeoPath = config->json["skopeoPath"].GetString();
    auto skopeoArgs = common::CLIArguments{skopeoPath};

    auto skopeoVerbosity = utility::getSkopeoVerbosityOption();
    if (!skopeoVerbosity.empty()) {
        skopeoArgs.push_back(skopeoVerbosity);
    }

    auto ociImagePath = common::makeUniquePathWithRandomSuffix(config->directories.temp / "ociImage");
    printLog( boost::format("Creating temporary OCI image in: %s") % ociImagePath, common::LogLevel::GENERAL);
    skopeoArgs += common::CLIArguments{"copy",
                                       "docker-archive:"+imageArchive.string(),
                                       "oci:"+ociImagePath.string()+":sarus-oci-image"};

    start = std::chrono::system_clock::now();
    auto status = common::forkExecWait(skopeoArgs);
    if(status != 0) {
        auto message = boost::format("%s exited with code %d") % skopeoArgs % status;
        printLog(message, common::LogLevel::INFO);
        exit(status);
    }

    end = std::chrono::system_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / double(1000);

    printLog(boost::format("Elapsed time on loading    : %s [sec]") % elapsed, common::LogLevel::INFO);

    return OCIImage{config, ociImagePath};
}

void Loader::printLog(const boost::format &message, common::LogLevel LogLevel,
                      std::ostream& outStream, std::ostream& errStream) const {
    common::Logger::getInstance().log(message.str(), sysname, LogLevel, outStream, errStream);
}

} // namespace
} // namespace
