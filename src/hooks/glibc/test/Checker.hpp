/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_glibc_test_Checker_hpp
#define sarus_hooks_glibc_test_Checker_hpp

#include <string>
#include <vector>
#include <unistd.h>
#include <sys/mount.h>

#include "hooks/common/Utility.hpp"
#include "hooks/glibc/GlibcHook.hpp"
#include "test_utility/Misc.hpp"
#include "test_utility/config.hpp"
#include "test_utility/filesystem.hpp"
#include "test_utility/OCIHooks.hpp"

#include <CppUTest/TestHarness.h> // boost library must be included before CppUTest


namespace rj = rapidjson;

namespace sarus {
namespace hooks {
namespace glibc {
namespace test {

class Checker {
public:
    Checker& addHostLibcAndSymlink( const boost::filesystem::path& dummyLib,
                                    const boost::filesystem::path& hostLib,
                                    const boost::filesystem::path& hostSymlink) {
        sarus::common::createFoldersIfNecessary((bundleDir / hostLib).parent_path());
        sarus::common::createFoldersIfNecessary((bundleDir / hostSymlink).parent_path());
        boost::filesystem::copy_file(dummyLibsDir / dummyLib, bundleDir / hostLib);
        boost::filesystem::create_symlink(bundleDir / hostLib, bundleDir / hostSymlink);
        hostLibs.push_back(bundleDir / hostSymlink);
        return *this;
    }

    Checker& addHostLib(const boost::filesystem::path& dummyLib,
                        const boost::filesystem::path& hostLib) {
        sarus::common::createFoldersIfNecessary((bundleDir / hostLib).parent_path());
        boost::filesystem::copy_file(dummyLibsDir / dummyLib, bundleDir / hostLib);
        hostLibs.push_back(bundleDir / hostLib);
        return *this;
    }

    Checker& addContainerLibcAndSymlink(const boost::filesystem::path& dummyLib,
                                        const boost::filesystem::path& containerLib,
                                        const boost::filesystem::path& containerSymlink,
                                        const boost::filesystem::path& expectedContainerLibAfterInjection) {
        sarus::common::createFoldersIfNecessary((rootfsDir / containerLib).parent_path());
        sarus::common::createFoldersIfNecessary((rootfsDir / containerSymlink).parent_path());
        boost::filesystem::copy_file(dummyLibsDir / dummyLib, rootfsDir / containerLib);
        boost::filesystem::create_symlink(rootfsDir / containerLib, rootfsDir / containerSymlink);
        containerLibs.push_back(rootfsDir / containerSymlink);
        expectedContainerLibsAfterInjection.push_back(dummyLibsDir / expectedContainerLibAfterInjection);
        return *this;
    }

    Checker& addContainerLib(   const boost::filesystem::path& dummyLib,
                                const boost::filesystem::path& containerLib,
                                const boost::filesystem::path& expectedContainerLibAfterInjection) {
        sarus::common::createFoldersIfNecessary((rootfsDir / containerLib).parent_path());
        boost::filesystem::copy_file(dummyLibsDir / dummyLib, rootfsDir / containerLib);
        containerLibs.push_back(rootfsDir / containerLib);
        expectedContainerLibsAfterInjection.push_back(dummyLibsDir / expectedContainerLibAfterInjection);
        return *this;
    }

    Checker& runLdconfigInContainer() {
        sarus::common::createFoldersIfNecessary(rootfsDir / "etc");
        sarus::common::executeCommand("echo '/lib' >> " + (rootfsDir / "etc/ld.so.conf").string());
        sarus::common::executeCommand("echo '/lib64' >> " + (rootfsDir / "etc/ld.so.conf").string());
        sarus::common::executeCommand("ldconfig -r " + rootfsDir.string()); // run ldconfig in rootfs
        return *this;
    }

    void checkSuccess() const {
        setupTestEnvironment();
        GlibcHook{}.injectGlibcLibrariesIfNecessary();
        checkContainerLibraries();
        cleanup();
    }

    void checkFailure() const {
        setupTestEnvironment();
        try {
            GlibcHook{}.injectGlibcLibrariesIfNecessary();
        }
        catch(...) {
            boost::filesystem::remove_all(bundleDir); // cleanup
            return;
        }
        boost::filesystem::remove_all(bundleDir); // cleanup
        CHECK(false); // expected failure/exception didn't occur
    }

private:
    void setupTestEnvironment() const {
        auto doc = test_utility::ocihooks::createBaseConfigJSON(rootfsDir, test_utility::misc::getNonRootUserIds());
        sarus::common::writeJSON(doc, bundleDir / "config.json");
        test_utility::ocihooks::writeContainerStateToStdin(bundleDir);

        sarus::common::setEnvironmentVariable("SARUS_GLIBC_LDCONFIG_PATH=ldconfig");
        sarus::common::setEnvironmentVariable("SARUS_GLIBC_READELF_PATH=readelf");
        sarus::common::setEnvironmentVariable("SARUS_GLIBC_LIBS="
            + sarus::common::makeColonSeparatedListOfPaths(hostLibs));
    }

    void checkContainerLibraries() const {
        CHECK_EQUAL(containerLibs.size(), expectedContainerLibsAfterInjection.size());
        for(size_t i=0; i<containerLibs.size(); ++i) {
            if(!expectedContainerLibsAfterInjection[i].empty()) {
                CHECK(test_utility::filesystem::areFilesEqual(containerLibs[i], expectedContainerLibsAfterInjection[i]));
            }
        }
    }

    void cleanup() const {
        for(const auto& lib : containerLibs) {
            umount(lib.c_str());
        }
        boost::filesystem::remove_all(bundleDir);
    }

private:
    sarus::common::Config config = test_utility::config::makeConfig();
    boost::filesystem::path bundleDir = boost::filesystem::path{ config.json["OCIBundleDir"].GetString() };
    boost::filesystem::path rootfsDir = bundleDir / config.json["rootfsFolder"].GetString();
    boost::filesystem::path dummyLibsDir = boost::filesystem::path{__FILE__}
        .parent_path()
        .parent_path()
        .parent_path()
        .parent_path()
        .parent_path() / "CI/dummy_libs";

    std::vector<boost::filesystem::path> hostLibs;
    std::vector<boost::filesystem::path> containerLibs;
    std::vector<boost::filesystem::path> expectedContainerLibsAfterInjection;
};

}}}} // namespace

#endif
