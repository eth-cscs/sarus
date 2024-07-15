/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_utility_filesystem_hpp
#define libsarus_utility_filesystem_hpp

#include <string>
#include <tuple>
#include <vector>
#include <sys/types.h>

#include <boost/filesystem.hpp>

/**
 * Utility functions for filesystem manipulation and investigation
 */

namespace libsarus {
namespace filesystem {

std::tuple<uid_t, gid_t> getOwner(const boost::filesystem::path&);
void setOwner(const boost::filesystem::path&, uid_t, gid_t);
void createFoldersIfNecessary(const boost::filesystem::path&, uid_t uid=-1, gid_t gid=-1);
void createFileIfNecessary(const boost::filesystem::path&, uid_t uid=-1, gid_t gid=-1);
void copyFile(const boost::filesystem::path& src, const boost::filesystem::path& dst, uid_t uid=-1, gid_t gid=-1);
void removeFile(const boost::filesystem::path& path);
void copyFolder(const boost::filesystem::path& src, const boost::filesystem::path& dst, uid_t uid=-1, gid_t gid=-1);
void changeDirectory(const boost::filesystem::path& path);
size_t getFileSize(const boost::filesystem::path& filename);
int countFilesInDirectory(const boost::filesystem::path& path);
std::string readFile(const boost::filesystem::path& path);
void writeTextFile(const std::string& text,
                   const boost::filesystem::path& filename,
                   const std::ios_base::openmode mode = std::ios_base::out);
boost::filesystem::path makeUniquePathWithRandomSuffix(const boost::filesystem::path&);
std::string makeColonSeparatedListOfPaths(const std::vector<boost::filesystem::path>& paths);
boost::filesystem::path appendPathsWithinRootfs( const boost::filesystem::path& rootfs,
                                                 const boost::filesystem::path& path0,
                                                 const boost::filesystem::path& path1,
                                                 std::vector<boost::filesystem::path>* traversedSymlinks = nullptr); 
boost::filesystem::path realpathWithinRootfs(const boost::filesystem::path& rootfs, const boost::filesystem::path& path);
dev_t getDeviceID(const boost::filesystem::path& path);
char getDeviceType(const boost::filesystem::path& path);

bool isDeviceFile(const boost::filesystem::path& path);
bool isBlockDevice(const boost::filesystem::path& path);
bool isCharacterDevice(const boost::filesystem::path& path);
bool isSymlink(const boost::filesystem::path& path);
bool isLibc(const boost::filesystem::path&);
bool isSharedLib(const boost::filesystem::path& file);

}}

#endif
