/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_image_manger_SkopeoDriver_hpp
#define sarus_image_manger_SkopeoDriver_hpp

#include <iostream>
#include <string>

#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include "common/Config.hpp"
#include "common/Lockfile.hpp"
#include "common/LogLevel.hpp"
#include "common/CLIArguments.hpp"


namespace sarus {
namespace image_manager {

class SkopeoDriver {
public:
    SkopeoDriver(std::shared_ptr<const common::Config> config);
    ~SkopeoDriver();
    boost::filesystem::path copyToOCIImage(const std::string& sourceTransport, const std::string& sourceReference) const;
    std::string inspectRaw(const std::string& sourceTransport, const std::string& sourceReference) const;
    std::string manifestDigest(const boost::filesystem::path& manifestPath) const;
    boost::filesystem::path acquireAuthFile(const common::Config::Authentication& auth, const common::ImageReference& reference);
    common::CLIArguments generateBaseArgs() const;

private:
    std::string getVerbosityOption() const;
    common::CLIArguments getPolicyOption() const;
    common::CLIArguments getRegistriesDOption() const;
    std::string getTransportString(const std::string& transport) const;
    void printLog(const boost::format &message, common::LogLevel,
                  std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr) const;
    void printLog(const std::string& message, common::LogLevel,
                  std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr) const;

private:
    boost::filesystem::path skopeoPath;
    boost::filesystem::path tempDir;
    boost::filesystem::path cachePath;
    boost::filesystem::path customPolicyPath;
    boost::filesystem::path customRegistriesDPath;
    boost::filesystem::path authFileBasePath;
    boost::filesystem::path authFilePath;
    bool enforceCustomPolicy;
    const std::string sysname = "SkopeoDriver";
};

}
}

#endif
