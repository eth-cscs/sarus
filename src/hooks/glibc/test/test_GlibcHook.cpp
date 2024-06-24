/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "libsarus/Logger.hpp"
#include "Checker.hpp"
#include "test_utility/unittest_main_function.hpp"


namespace sarus {
namespace hooks {
namespace glibc {
namespace test {


TEST_GROUP(GlibcHookTestGroup) {
};

TEST(GlibcHookTestGroup, test) {
    // no glibc libraries in host
    Checker{}
        .runLdconfigInContainer()
        .checkFailure();

    // no libc library in host
    Checker{}
        .addHostLib("ld-linux-x86-64.so.2-host", "/lib64/ld-linux-x86-64.so.2")
        .checkFailure();

    // no glibc in container (e.g. Alpine Linux)
    // note that here we don't run ldconfig in the container, i.e. we don't generate /etc/ld.so.cache in the container
    // and the hook assumes that the container doesn't have glibc
    Checker{}
        .addHostLibcAndSymlink("libc.so.6-host", "/lib64/libc-2.26.so", "/lib64/libc.so.6")
        .checkSuccess();

    // no libc library in container (it is possible in a container with only 32-bit glibc)
    Checker{}
        .addHostLibcAndSymlink("libc.so.6-host", "/lib64/libc-2.26.so", "/lib64/libc.so.6")
        .addHostLib("ld-linux-x86-64.so.2-host", "/lib64/ld-linux-x86-64.so.2")
        .runLdconfigInContainer()
        .checkSuccess();

    // no libc library in container (only 32-bit libc in container)
    Checker{}
        .addHostLibcAndSymlink("libc.so.6-host", "/lib64/libc-2.26.so", "/lib64/libc.so.6")
        .addHostLib("ld-linux-x86-64.so.2-host", "/lib64/ld-linux-x86-64.so.2")
        .addContainerLibcAndSymlink("libc.so.6-32bit-container", "/lib/libc-2.25.so", "/lib/libc.so.6", "libc.so.6-32bit-container")
        .runLdconfigInContainer()
        .checkSuccess();

    // incompatible libraries (different ABI string)
    Checker{}
        .addHostLibcAndSymlink("libc.so.6-host", "/lib64/libc-2.26.so", "/lib64/libc.so.6")
        .addContainerLibcAndSymlink("libc.so.5-container", "/lib64/libc-2.25.so", "/lib64/libc.so.5", {})
        .runLdconfigInContainer()
        .mockLddWithOlderVersion()
        .checkFailure();

    // no glibc injection needed (container's glibc version == host's glibc version)
    Checker{}
        .addHostLibcAndSymlink("libc.so.6-host", "/lib64/libc-2.26.so", "/lib64/libc.so.6")
        .addContainerLibcAndSymlink("libc.so.6-container", "/lib64/libc-2.26.so", "/lib64/libc.so.6", "libc.so.6-container")
        .runLdconfigInContainer()
        .mockLddWithEqualVersion()
        .checkSuccess();

    // no glibc injection needed (container's glibc version > host's glibc version)
    Checker{}
        .addHostLibcAndSymlink("libc.so.6-host", "/lib64/libc-2.26.so", "/lib64/libc.so.6")
        .addContainerLibcAndSymlink("libc.so.6-container", "/lib64/libc-2.27.so", "/lib64/libc.so.6", "libc.so.6-container")
        .runLdconfigInContainer()
        .mockLddWithNewerVersion()
        .checkSuccess();

    // glibc replacement of single library
    Checker{}
        .addHostLibcAndSymlink("libc.so.6-host", "/lib64/libc-2.26.so", "/lib64/libc.so.6")
        .addContainerLibcAndSymlink("libc.so.6-container", "/lib64/libc-2.25.so", "/lib64/libc.so.6", "libc.so.6-host")
        .runLdconfigInContainer()
        .mockLddWithOlderVersion()
        .checkSuccess();

    // glibc replacement and addition of libraries
    Checker{}
        .addHostLibcAndSymlink("libc.so.6-host", "/lib64/libc-2.26.so", "/lib64/libc.so.6")
        .addHostLib("ld-linux-x86-64.so.2-host", "/lib64/ld-linux-x86-64.so.2")
        .addContainerLibcAndSymlink("libc.so.6-container", "/lib64/libc-2.25.so", "/lib64/libc.so.6", "libc.so.6-host")
        .expectContainerLib("/lib64/ld-linux-x86-64.so.2", "ld-linux-x86-64.so.2-host")
        .runLdconfigInContainer()
        .mockLddWithOlderVersion()
        .checkSuccess();

    // glibc replacement of multiple libraries
    Checker{}
        .addHostLibcAndSymlink("libc.so.6-host", "/lib64/libc-2.26.so", "/lib64/libc.so.6")
        .addHostLib("ld-linux-x86-64.so.2-host", "/lib64/ld-linux-x86-64.so.2")
        .addContainerLibcAndSymlink("libc.so.6-container", "/lib64/libc-2.25.so", "/lib64/libc.so.6", "libc.so.6-host")
        .addContainerLib("ld-linux-x86-64.so.2-container", "/lib64/ld-linux-x86-64.so.2", "ld-linux-x86-64.so.2-host")
        .runLdconfigInContainer()
        .mockLddWithOlderVersion()
        .checkSuccess();

    // mixed 32-bit and 64-bit libraries in container
    Checker{}
        .addHostLibcAndSymlink("libc.so.6-host", "/lib64/libc-2.26.so", "/lib64/libc.so.6")
        .addHostLib("ld-linux-x86-64.so.2-host", "/lib64/ld-linux-x86-64.so.2")
        .addContainerLibcAndSymlink("libc.so.6-32bit-container", "/lib/libc-2.25.so", "/lib/libc.so.6", "libc.so.6-32bit-container")
        .addContainerLibcAndSymlink("libc.so.6-container", "/lib64/libc-2.25.so", "/lib64/libc.so.6", "libc.so.6-host")
        .expectContainerLib("/lib64/ld-linux-x86-64.so.2", "ld-linux-x86-64.so.2-host")
        .runLdconfigInContainer()
        .mockLddWithOlderVersion()
        .checkSuccess();
}

}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
