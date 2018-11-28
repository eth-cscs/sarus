/**
 * @brief Filesystem utility functions to be used in the tests.
 */

#include "filesystem.hpp"

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <cstdlib>
#include <vector>
#include <string>

#include "common/Utility.hpp"


namespace test_utility {
namespace filesystem {

bool are_directories_equal(const std::string& dir1, const std::string& dir2, bool compare_file_attributes)
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
        if (!are_directories_equal(subdirs1[i], subdirs2[i], compare_file_attributes)) {
            ret = false;
        }
    }

    return ret;
}

bool areFilesEqual(const boost::filesystem::path& file0, const boost::filesystem::path& file1) {
    std::string command = "diff " + file0.string() + " " + file1.string();
    return std::system(command.c_str()) == 0;
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

void create_test_directory_tree(const std::string& dir) {
    sarus::common::executeCommand("mkdir -p " + dir);
    sarus::common::executeCommand("touch " + dir + "/a.txt");
    sarus::common::executeCommand("touch " + dir + "/b.md");
    sarus::common::executeCommand("touch " + dir + "/c.h");
    sarus::common::executeCommand("chmod 755 " + dir + "/a.txt");
    sarus::common::executeCommand("chmod 644 " + dir + "/b.md");
    sarus::common::executeCommand("chmod 700 " + dir + "/c.h");

    sarus::common::executeCommand("mkdir -p " + dir + "/sub1");
    sarus::common::executeCommand("touch " + dir + "/sub1/d.cpp");
    sarus::common::executeCommand("touch " + dir + "/sub1/e.so");
    sarus::common::executeCommand("chmod 600 " + dir + "/sub1/d.cpp");
    sarus::common::executeCommand("chmod 775 " + dir + "/sub1/e.so");

    sarus::common::executeCommand("mkdir -p " + dir + "/sub1/ssub11");
    sarus::common::executeCommand("touch " + dir + "/sub1/ssub11/g.pdf");
    sarus::common::executeCommand("touch " + dir + "/sub1/ssub11/h.py");
    sarus::common::executeCommand("chmod 665 " + dir + "/sub1/ssub11/g.pdf");
    sarus::common::executeCommand("chmod 777 " + dir + "/sub1/ssub11/h.py");

    sarus::common::executeCommand("mkdir -p " + dir + "/sub2");
    sarus::common::executeCommand("touch " + dir + "/sub2/f.a");
    sarus::common::executeCommand("chmod 666 " + dir + "/sub2/f.a");
}

} // filesystem
} // test_utility