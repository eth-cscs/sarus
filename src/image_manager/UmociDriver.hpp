/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_image_manger_UmociDriver_hpp
#define sarus_image_manger_UmociDriver_hpp

#include <iostream>
#include <string>

#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "common/Config.hpp"
#include "common/LogLevel.hpp"
#include "common/CLIArguments.hpp"


namespace sarus {
namespace image_manager {

class UmociDriver {
public:
    UmociDriver(std::shared_ptr<const common::Config> config);
    void unpack(const boost::filesystem::path& imagePath, const boost::filesystem::path& expansionPath) const;
    common::CLIArguments generateBaseArgs() const;

private:
    std::string getVerbosityOption() const;
    void printLog(const boost::format &message, common::LogLevel,
                  std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr) const;
    void printLog(const std::string& message, common::LogLevel,
                  std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr) const;

private:
    boost::filesystem::path umociPath;
    const std::string sysname = "UmociDriver";
};

}
}

#endif
