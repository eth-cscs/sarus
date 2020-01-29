/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _SarusImage_hpp
#define _SarusImage_hpp

#include <string>
#include <boost/filesystem.hpp>

#include "common/ImageID.hpp"


namespace sarus {
namespace common {

struct SarusImage {
    common::ImageID imageID;
    std::string digest;                 // The digest of the container image
    std::string datasize;               // The size of container image file
    std::string created;                // The time creation time
    boost::filesystem::path imageFile;
    boost::filesystem::path metadataFile;

    static std::string createTimeString(time_t time_in);
    static std::string createSizeString(size_t size);
};

bool operator==(const SarusImage&, const SarusImage&);

}
}

#endif
