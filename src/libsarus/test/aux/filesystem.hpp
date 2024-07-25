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

#ifndef libsarus_test_aux_filesystem_hpp
#define libsarus_test_aux_filesystem_hpp

#include <fcntl.h>
#include <string>

#include <boost/filesystem.hpp>


namespace libsarus {
namespace test {
namespace aux {
namespace filesystem {

bool areDirectoriesEqual(const std::string& dir1, const std::string& dir2, bool compare_file_attributes);
bool isSameBindMountedFile(const boost::filesystem::path&, const boost::filesystem::path&);
void createCharacterDeviceFile(const boost::filesystem::path& path,
                               const int majorID,
                               const int minorID,
                               const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
void createBlockDeviceFile(const boost::filesystem::path& path,
                               const int majorID,
                               const int minorID,
                               const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
void createTestDirectoryTree(const std::string& dir);

}}}}

#endif
