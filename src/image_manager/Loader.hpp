/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _ImageManager_Loader_hpp
#define _ImageManager_Loader_hpp

#include <string>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "common/Config.hpp"
#include "common/Logger.hpp"
#include "image_manager/LoadedImage.hpp"


namespace sarus {
namespace image_manager {

class Loader {
public:
    Loader(std::shared_ptr<const common::Config> config);
    LoadedImage load(const boost::filesystem::path& imageArchive);

private:
    void printLog(const boost::format &message, common::LogLevel LogLevel,
                  std::ostream& outStream = std::cout, std::ostream& errStream = std::cerr) const;

private:
    std::shared_ptr<const common::Config> config;

    /** system name for logger */
    const std::string sysname = "Loader";
};

}
}

#endif
