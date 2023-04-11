/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_image_manger_Utility_hpp
#define sarus_image_manger_Utility_hpp

#include <string>

#include <boost/format.hpp>
#include <rapidjson/document.h>

#include "common/Logger.hpp"

namespace sarus {
namespace image_manager {
namespace utility {

rapidjson::Document getCurrentOCIPlatform();
std::string getPlatformDigestFromOCIIndex(const rapidjson::Document& index, const rapidjson::Document& targetPlatform);
std::string base64Encode(const std::string& input);

void printLog(const boost::format& message, common::LogLevel LogLevel,
              std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr);
void printLog(const std::string& message, common::LogLevel LogLevel,
              std::ostream& outStream=std::cout, std::ostream& errStream=std::cerr);

}
}
}

#endif
