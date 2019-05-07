/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <sys/vfs.h>
#include <sys/mount.h>
#include <linux/magic.h>

#include "test_utility/config.hpp"
#include "common/Logger.hpp"
#include "common/Config.hpp"
#include "common/Utility.hpp"
#include "runtime/Runtime.hpp"
#include "test_utility/unittest_main_function.hpp"


using namespace sarus;

TEST_GROUP(RuntimeTestGroup) {
};

__fsword_t getFilesystemType(const boost::filesystem::path& path) {
    struct statfs sb;
    if(statfs(path.c_str(), &sb) != 0) {
        auto message = boost::format("Failed to statfs %s") % path;
        SARUS_THROW_ERROR(message.str());
    }
    return sb.f_type;
}

__fsword_t getExpectedFilesystemTypeOfBundle(const common::Config& config) {
    auto type = std::string{config.json.get()["ramFilesystemType"].GetString()};
    if(type == "tmpfs") {
        return TMPFS_MAGIC;
    }
    else if(type == "ramfs") {
        return RAMFS_MAGIC;
    }
    else {
        SARUS_THROW_ERROR("build-time configuration contains unexpected ramFilesystemType");
    }
}

#ifdef NOTROOT
IGNORE_TEST(RuntimeTestGroup, setupOCIBundle) {
#else
TEST(RuntimeTestGroup, setupOCIBundle) {
#endif
    // configure
    auto config = std::make_shared<common::Config>(test_utility::config::makeConfig());
    config->commandRun.execArgs = common::CLIArguments{"/bin/bash"};
    // hack to make the resulting image's file path = <repository dir>////test-image.squashfs
    config->directories.images = boost::filesystem::path{__FILE__}.parent_path();
    config->imageID = common::ImageID{"", "", "", "test_image"};

    auto bundleDir = boost::filesystem::path{config->json.get()["OCIBundleDir"].GetString()};
    auto overlayfsLowerDir = bundleDir / "overlay/rootfs-lower"; // hardcoded so in production code being tested
    auto rootfsDir = bundleDir / boost::filesystem::path{config->json.get()["rootfsFolder"].GetString()};
    auto dirOfFilesToCopyInContainerEtc = boost::filesystem::path{config->json.get()["dirOfFilesToCopyInContainerEtc"].GetString()};

    // create test folders / files
    common::createFoldersIfNecessary(bundleDir);
    common::createFileIfNecessary(dirOfFilesToCopyInContainerEtc / "nsswitch.conf");
    common::createFileIfNecessary(dirOfFilesToCopyInContainerEtc / "passwd");
    common::createFileIfNecessary(dirOfFilesToCopyInContainerEtc / "group");

    // create dummy metadata file in image repo
    auto metadataFile = boost::filesystem::path(config->directories.images / (config->imageID.getUniqueKey() + ".meta"));
    common::createFileIfNecessary(metadataFile);
    std::ofstream metadataStream(metadataFile.c_str());
    metadataStream << "{}";
    metadataStream.close();

    // run
    runtime::Runtime{config}.setupOCIBundle();

    // check filesystem types
    CHECK_EQUAL(getFilesystemType(bundleDir), getExpectedFilesystemTypeOfBundle(*config));
    #ifdef OVERLAYFS_SUPER_MAGIC // skip the check if not defined (e.g. on Cray)
    CHECK_EQUAL(getFilesystemType(rootfsDir), OVERLAYFS_SUPER_MAGIC);
    #endif

    // check file from image in rootfs
    CHECK(boost::filesystem::exists(rootfsDir / "file_in_squashfs_image"));

    // check etc files in rootfs
    CHECK(boost::filesystem::exists(rootfsDir / "etc/hosts"));
    CHECK(boost::filesystem::exists(rootfsDir / "etc/resolv.conf"));
    CHECK(boost::filesystem::exists(rootfsDir / "etc/nsswitch.conf"));
    CHECK(boost::filesystem::exists(rootfsDir / "etc/passwd"));
    CHECK(boost::filesystem::exists(rootfsDir / "etc/group"));

    // check that rootfs is writable
    auto fileToCreate = rootfsDir / "file_to_create";
    common::executeCommand("touch " + fileToCreate.string());

    // check that bundle's config file exists
    CHECK(boost::filesystem::exists(bundleDir / "config.json"));

    // cleanup
    CHECK_EQUAL(umount((rootfsDir / "dev").c_str()), 0);
    CHECK_EQUAL(umount(rootfsDir.c_str()), 0);
    CHECK_EQUAL(umount(overlayfsLowerDir.c_str()), 0);
    CHECK_EQUAL(umount(bundleDir.c_str()), 0);
    boost::filesystem::remove_all(bundleDir);
    boost::filesystem::remove_all(dirOfFilesToCopyInContainerEtc);
}

SARUS_UNITTEST_MAIN_FUNCTION();
