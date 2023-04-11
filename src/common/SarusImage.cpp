/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/SarusImage.hpp"

#include <vector>
#include <ctime>
#include <boost/format.hpp>


namespace sarus {
namespace common {

std::string SarusImage::createTimeString(time_t time_in)
{
    struct tm * time_info = localtime(&time_in);
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", time_info);
    return std::string(buffer);
}

std::string SarusImage::createSizeString(size_t size)
{
    const std::vector<std::string> suffix = {"B", "KB", "MB", "GB", "TB"};
    const double unit(1024);
    
    double size_d(size);
    size_t i = 0;

    while ( (size_d > unit) && (i < (suffix.size() - 1) ) )
    {
        size_d = size_d / unit;
        ++i;
    }
    return ( boost::format("%.2f%s") % size_d % suffix[i] ).str();
}

bool operator==(const SarusImage& lhs, const SarusImage& rhs) {
    return lhs.reference == rhs.reference
        && lhs.id == rhs.id
        && lhs.datasize == rhs.datasize
        && lhs.created == rhs.created;
}

} // namespace
} // namespace
