/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/**
 * @brief Filesystem utility functions to be used in the tests.
 */

#ifndef sarus_test_utility_filesystem_hpp
#define sarus_test_utility_filesystem_hpp

#include <string>
#include <fcntl.h>
#include <boost/filesystem.hpp>


namespace test_utility {
namespace filesystem {

bool are_directories_equal(const std::string& dir1, const std::string& dir2, bool compare_file_attributes);
bool areFilesEqual(const boost::filesystem::path&, const boost::filesystem::path&);
bool isSameBindMountedFile(const boost::filesystem::path&, const boost::filesystem::path&);
void createCharacterDeviceFile(const boost::filesystem::path& path,
                               const int majorID,
                               const int minorID,
                               const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
void createBlockDeviceFile(const boost::filesystem::path& path,
                               const int majorID,
                               const int minorID,
                               const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
void create_test_directory_tree(const std::string& dir);

}
}

#endif
