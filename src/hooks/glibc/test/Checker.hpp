/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_glibc_test_Checker_hpp
#define sarus_hooks_glibc_test_Checker_hpp

#include <string>
#include <vector>
#include <unordered_map>
#include <unistd.h>
#include <sys/mount.h>

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
        libsarus::filesystem::createFoldersIfNecessary((bundleDir / hostLib).parent_path());
        libsarus::filesystem::createFoldersIfNecessary((bundleDir / hostSymlink).parent_path());
        boost::filesystem::copy_file(dummyLibsDir / dummyLib, bundleDir / hostLib);
        boost::filesystem::create_symlink(bundleDir / hostLib, bundleDir / hostSymlink);
        hostLibs.push_back(bundleDir / hostSymlink);
        return *this;
    }

    Checker& addHostLib(const boost::filesystem::path& dummyLib,
                        const boost::filesystem::path& hostLib) {
        libsarus::filesystem::createFoldersIfNecessary((bundleDir / hostLib).parent_path());
        boost::filesystem::copy_file(dummyLibsDir / dummyLib, bundleDir / hostLib);
        hostLibs.push_back(bundleDir / hostLib);
        return *this;
    }

    Checker& addContainerLibcAndSymlink(const boost::filesystem::path& dummyLib,
                                        const boost::filesystem::path& containerLib,
                                        const boost::filesystem::path& containerSymlink,
                                        const boost::filesystem::path& expectedContainerLibAfterInjection) {
        libsarus::filesystem::createFoldersIfNecessary((rootfsDir / containerLib).parent_path());
        libsarus::filesystem::createFoldersIfNecessary((rootfsDir / containerSymlink).parent_path());
        boost::filesystem::copy_file(dummyLibsDir / dummyLib, rootfsDir / containerLib);
        boost::filesystem::create_symlink(rootfsDir / containerLib, rootfsDir / containerSymlink);
        expectContainerLib(containerSymlink, expectedContainerLibAfterInjection);
        return *this;
    }

    Checker& addContainerLib(   const boost::filesystem::path& dummyLib,
                                const boost::filesystem::path& containerLib,
                                const boost::filesystem::path& expectedContainerLibAfterInjection) {
        libsarus::filesystem::createFoldersIfNecessary((rootfsDir / containerLib).parent_path());
        boost::filesystem::copy_file(dummyLibsDir / dummyLib, rootfsDir / containerLib);
        expectContainerLib(containerLib, expectedContainerLibAfterInjection);
        return *this;
    }

    Checker& expectContainerLib(const boost::filesystem::path& containerLib,
                                const boost::filesystem::path& expectedContainerLibAfterInjection) {
        containerLibs.push_back(rootfsDir / containerLib);
        expectedContainerLibsAfterInjection.push_back(dummyLibsDir / expectedContainerLibAfterInjection);
        return *this;
    }

    Checker& runLdconfigInContainer() {
        libsarus::filesystem::createFoldersIfNecessary(rootfsDir / "etc");
        libsarus::process::executeCommand("echo '/lib' >> " + (rootfsDir / "etc/ld.so.conf").string());
        libsarus::process::executeCommand("echo '/lib64' >> " + (rootfsDir / "etc/ld.so.conf").string());
        libsarus::process::executeCommand("ldconfig -r " + rootfsDir.string()); // run ldconfig in rootfs
        return *this;
    }

    Checker& mockLddWithOlderVersion() {
        libsarus::filesystem::copyFile(boost::filesystem::current_path() / "mocks/lddMockOlder", rootfsDir / lddPath);
        return *this;
    }

    Checker& mockLddWithEqualVersion() {
        libsarus::filesystem::copyFile(boost::filesystem::current_path() / "mocks/lddMockEqual", rootfsDir / lddPath);
        return *this;
    }

    Checker& mockLddWithNewerVersion() {
        libsarus::filesystem::copyFile(boost::filesystem::current_path() / "mocks/lddMockNewer", rootfsDir / lddPath);
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
            return;
        }
        CHECK(false); // expected failure/exception didn't occur
    }

private:
    void setupTestEnvironment() const {
        auto doc = test_utility::ocihooks::createBaseConfigJSON(rootfsDir, test_utility::misc::getNonRootUserIds());
        libsarus::json::write(doc, bundleDir / "config.json");
        test_utility::ocihooks::writeContainerStateToStdin(bundleDir);

        libsarus::environment::setVariable("LDD_PATH", (boost::filesystem::current_path() / "mocks/lddMockEqual").string());
        libsarus::environment::setVariable("LDCONFIG_PATH", "ldconfig");
        libsarus::environment::setVariable("READELF_PATH", "readelf");
        libsarus::environment::setVariable("GLIBC_LIBS", libsarus::filesystem::makeColonSeparatedListOfPaths(hostLibs));

        libsarus::filesystem::createFoldersIfNecessary(rootfsDir / "tmp",
                                                doc["process"]["user"]["uid"].GetInt(),
                                                doc["process"]["user"]["gid"].GetInt());
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
    }

private:
    test_utility::config::ConfigRAII configRAII = test_utility::config::makeConfig();
    boost::filesystem::path lddPath = boost::filesystem::path{"/usr/bin/ldd"};
    boost::filesystem::path bundleDir = boost::filesystem::path{ configRAII.config->json["OCIBundleDir"].GetString() };
    boost::filesystem::path rootfsDir = bundleDir / configRAII.config->json["rootfsFolder"].GetString();
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
