/*
 * Sarus
 * 
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 * 
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 * 
 **/

/**
 * @brief Filesystem utility functions to be used in the tests.
 **/

#include "filesystem.hpp"

#include <cstdlib>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "libsarus/Utility.hpp"


namespace libsarus {
namespace test {
namespace aux {
namespace filesystem {

bool areDirectoriesEqual(const std::string& dir1, const std::string& dir2, bool compare_file_attributes)
{
    struct dirent **namelist1, **namelist2;
    struct stat statData1, statData2;
    std::vector<std::string> subdirs1, subdirs2;
    bool ret = true;
    int n1, n2;

    // Retrieve sorted list of directories contents
    n1 = scandir(dir1.c_str(), &namelist1, NULL, alphasort);
    if (n1 == -1) {
        perror("scandir");
        return 1;
    }

    n2 = scandir(dir2.c_str(), &namelist2, NULL, alphasort);
    if (n2 == -1) {
        perror("scandir");
        return 1;
    }

    // Check number of files in both directories
    if (n1 != n2)
        ret = false;

    // Check directories contents
    for (int i=0; i<n1 && ret; ++i) {
        std::string filename1(namelist1[i]->d_name);
        std::string fullname1(dir1+"/"+filename1);

        std::string filename2(namelist2[i]->d_name);
        std::string fullname2(dir2+"/"+filename2);

        if (filename1 != filename2) {
            ret = false;
            break;
        }

        // Stat files
        if (stat(fullname1.c_str(), &statData1) != 0
                || stat(fullname2.c_str(), &statData2) != 0) {
            perror("stat");
            ret = false;
            break;
        }

        // If these are sub-directories, save their names for later
        if (S_ISDIR(statData1.st_mode)
                && filename1 != ".."
                && filename1 != ".") {
            subdirs1.push_back(fullname1);
            subdirs2.push_back(fullname2);
        }

        // If preservation of attributes was requested, check attributes
        if (compare_file_attributes && filename1 != ".." && filename1 != ".") {
            bool mode_check = ( (statData1.st_mode & 0000777)
                               != (statData2.st_mode & 0000777) );
            bool owner_check = (statData1.st_uid != statData2.st_uid);
            bool group_check = (statData1.st_gid != statData2.st_gid);

            if (mode_check || owner_check || group_check) {
                ret = false;
                break;
            }
        }
    }

    // Cleanup dirent structures
    while (n1--) {
        free(namelist1[n1]);
    }
    free(namelist1);

    while (n2--) {
        free(namelist2[n2]);
    }
    free(namelist2);

    //Descend into subdirectories
    for (uint i=0; i<subdirs1.size() && ret; ++i) {
        if (!areDirectoriesEqual(subdirs1[i], subdirs2[i], compare_file_attributes)) {
            ret = false;
        }
    }

    return ret;
}

static std::tuple<dev_t, ino_t> getDeviceIDAndINodeNumber(const boost::filesystem::path& path) {
    struct stat sb;
    if(stat(path.c_str(), &sb) != 0) {
        auto message = boost::format("Failed to stat %s: %s") % path % strerror(errno);
        SARUS_THROW_ERROR(message.str());
    }
    return std::tuple<dev_t, ino_t>{sb.st_dev, sb.st_ino};
}

bool isSameBindMountedFile(const boost::filesystem::path& file0, const boost::filesystem::path& file1) {
    return getDeviceIDAndINodeNumber(file0) == getDeviceIDAndINodeNumber(file1);
}

static int clearFileTypeBits(const mode_t fileMode) {
    return fileMode & ~(S_IFREG | S_IFCHR | S_IFBLK | S_IFIFO | S_IFSOCK);
}

static void createFilesystemNode(const boost::filesystem::path& path, const mode_t mode, const dev_t deviceID) {
    if (mknod(path.c_str(), mode, deviceID) != 0) {
        auto message = boost::format("Failed to mknod test device file %s: %s") % path % strerror(errno);
        SARUS_THROW_ERROR(message.str().c_str());
    }
}

void createCharacterDeviceFile(const boost::filesystem::path& path, const int majorID, const int minorID, const mode_t mode) {
    auto fileMode = clearFileTypeBits(mode);
    fileMode = S_IFCHR | mode;
    auto deviceID = makedev(majorID, minorID);
    createFilesystemNode(path, fileMode, deviceID);
}

void createBlockDeviceFile(const boost::filesystem::path& path, const int majorID, const int minorID, const mode_t mode) {
    auto fileMode = clearFileTypeBits(mode);
    fileMode = S_IFBLK | mode;
    auto deviceID = makedev(majorID, minorID);
    createFilesystemNode(path, fileMode, deviceID);
}

void createTestDirectoryTree(const std::string& dir) {
    libsarus::process::executeCommand("mkdir -p " + dir);
    libsarus::process::executeCommand("touch " + dir + "/a.txt");
    libsarus::process::executeCommand("touch " + dir + "/b.md");
    libsarus::process::executeCommand("touch " + dir + "/c.h");
    libsarus::process::executeCommand("chmod 755 " + dir + "/a.txt");
    libsarus::process::executeCommand("chmod 644 " + dir + "/b.md");
    libsarus::process::executeCommand("chmod 700 " + dir + "/c.h");

    libsarus::process::executeCommand("mkdir -p " + dir + "/sub1");
    libsarus::process::executeCommand("touch " + dir + "/sub1/d.cpp");
    libsarus::process::executeCommand("touch " + dir + "/sub1/e.so");
    libsarus::process::executeCommand("chmod 600 " + dir + "/sub1/d.cpp");
    libsarus::process::executeCommand("chmod 775 " + dir + "/sub1/e.so");

    libsarus::process::executeCommand("mkdir -p " + dir + "/sub1/ssub11");
    libsarus::process::executeCommand("touch " + dir + "/sub1/ssub11/g.pdf");
    libsarus::process::executeCommand("touch " + dir + "/sub1/ssub11/h.py");
    libsarus::process::executeCommand("chmod 665 " + dir + "/sub1/ssub11/g.pdf");
    libsarus::process::executeCommand("chmod 777 " + dir + "/sub1/ssub11/h.py");

    libsarus::process::executeCommand("mkdir -p " + dir + "/sub2");
    libsarus::process::executeCommand("touch " + dir + "/sub2/f.a");
    libsarus::process::executeCommand("chmod 666 " + dir + "/sub2/f.a");
}

}}}}
