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
namespace mpi {
namespace test {


TEST_GROUP(MPIHookTestGroup) {
};

TEST(MPIHookTestGroup, test_basics) {
    // no MPI libraries in host
    Checker{}
        .setHostMpiLibraries({})
        .setPreHookContainerLibraries({})
        .checkFailure();

    // no MPI libraries in container
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({})
        .checkFailure();
}

TEST(MPIHookTestGroup, test_mpi_libraries_injection) {
    // MPI library in non-default linker directory
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/local/lib/libmpi.so.12.5.5"})
        .expectPostHookContainerLibraries({
            "/usr/local/lib/libmpi.so.12.5.5",
            "/lib/libmpi.so", "/lib/libmpi.so.12", "/lib/libmpi.so.12.5", "/lib/libmpi.so.12.5.5",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12", "/lib64/libmpi.so.12.5", "/lib64/libmpi.so.12.5.5"})
        .checkSuccessful();

    // multiple host and container libraries, one version of each
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5", "/lib/libmpicxx.so.12.5.5"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.5.5", "/lib/libmpicxx.so.12.5.5"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12", "/lib/libmpi.so.12.5", "/lib/libmpi.so.12.5.5",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12", "/lib64/libmpi.so.12.5", "/lib64/libmpi.so.12.5.5",

            "/lib/libmpicxx.so", "/lib/libmpicxx.so.12", "/lib/libmpicxx.so.12.5", "/lib/libmpicxx.so.12.5.5",
            "/lib64/libmpicxx.so", "/lib64/libmpicxx.so.12", "/lib64/libmpicxx.so.12.5", "/lib64/libmpicxx.so.12.5.5"})
        .checkSuccessful();

    // multiple libraries (not all in container, but all injected)
    // Note: we inject all the host MPI libraries also when they are not present in the container because we don't
    // know about the dependencies between the host's MPI libraries. E.g. libmpicxx.so might depend on libmpi.so
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5", "/lib/libmpicxx.so.12.5.5"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.5.5"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12", "/lib/libmpi.so.12.5", "/lib/libmpi.so.12.5.5",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12", "/lib64/libmpi.so.12.5", "/lib64/libmpi.so.12.5.5",

            "/lib/libmpicxx.so", "/lib/libmpicxx.so.12", "/lib/libmpicxx.so.12.5", "/lib/libmpicxx.so.12.5.5",
            "/lib64/libmpicxx.so", "/lib64/libmpicxx.so.12", "/lib64/libmpicxx.so.12.5", "/lib64/libmpicxx.so.12.5.5"})
        .checkSuccessful();
}

TEST(MPIHookTestGroup, test_dependency_libraries_injection) {
    // no libdep.so in container => create it
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setHostMpiDependencyLibraries({"/lib/libdep.so"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12",

            "/lib/libdep.so",
            "/lib64/libdep.so"})
        .checkSuccessful();

    // container's libdep.so gets replaced with host's library
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setHostMpiDependencyLibraries({"/lib/libdep.so"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12", "/usr/local/lib/libdep.so"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12",

            "/usr/local/lib/libdep.so",
            "/lib/libdep.so",
            "/lib64/libdep.so"})
        .checkSuccessful();

    // multiple dep libraries in host get all injected (libdep0.so, libdep1.so)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setHostMpiDependencyLibraries({"/lib/libdep0.so", "/lib/libdep1.so"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12",

            "/lib/libdep0.so",
            "/lib64/libdep0.so",

            "/lib/libdep1.so",
            "/lib64/libdep1.so"})
        .checkSuccessful();

    // symlinks already exist (are replaced by the hook)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setHostMpiDependencyLibraries({"/lib/libdep.so"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12", "/lib/libdep.so", "/lib64/libdep.so"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12",

            "/lib/libdep.so",
            "/lib64/libdep.so"})
        .checkSuccessful();
}

TEST(MPIHookTestGroup, test_libraries_injection_container_version_matching) {
    /*  NOTES:
        - To properly check compatbility, the HOST and CONTAINER libraries should be defined with at least 2 of the 3 version numbers.
          e.g. libmpi.so.12.1 or libmpi.so.12.2.3, or even libmpi.so.12.0 not just libmpi.so.12.
        - This means, a CONTAINER library can be seen as OLDER, EQUAL or NEWER compared to the HOST version.
          e.g. for HOST version libmpi.so.12.1, CONTAINER libs libmpi.so.12.0, libmpi.so.12.1 and libmpi.so.13.1 are OLDER, EQUAL and NEWER respectively.

        TESTS:
        This test checks the policy defined to handle the case when more than one library version is found in the container.
        Granted, it is a weird case, but it came from a real Sarus user. The container had multiple versions of a "dependecy" library (libgfortran), but
        we prepare the tests for both MPI and dependencies libraries.

        The HOST is configured to have only 1 version of each library. But the container could bring more than one.
        When more than one version is available in the container, we have the following possible usecases in the CONTAINER:

        0 to N older (than HOST) versions
        0 or 1 equal (as HOST) version
        0 to N newer (than HOST) versions

        The HOOK injection will take this policy:
        - Only one library version will be injected from the host.
        - If the same (equal) is available in the container, it will be replaced. The rest of the libs in the container remain untouched.
        - Otherwise the newest of the older libraries is "chosen".
            If this is ABI compatible with the host, the container library is replaced.
            Otherwise, the host library is injected and the container libraries remain untouched.
        - Otherwise (only newer versions in container):
            - A warning is printed.
            - The host library is injected and both the libs and symlinks in the container remain untouched.
        The full chain of symlinks from linkername to lib is updated only when all container lib versions are ABI compatible with the host one.
    */
    // 2 older
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.3"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.1", "/lib/libmpi.so.12.2"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12", "/lib/libmpi.so.12.3",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12", "/lib64/libmpi.so.12.3",

            "/lib/libmpi.so.12.1",
            "/lib/libmpi.so.12.2"
        })
        .expectPreservedPostHookContainerLibraries({
            "/lib/libmpi.so.12.1"
        })
        .checkSuccessful();

    // 2 older 1 equal
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.1.1", "/lib/libmpi.so.12.5", "/lib/libmpi.so.12.2.2"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12", "/lib/libmpi.so.12.5",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12", "/lib64/libmpi.so.12.5",

            "/lib/libmpi.so.12.1.1",
            "/lib/libmpi.so.12.2.2"
        })
        .expectPreservedPostHookContainerLibraries({
            "/lib/libmpi.so.12.1.1",
            "/lib/libmpi.so.12.2.2"
        })
        .checkSuccessful();

    // NOTE: Container can't have an incompatible MPI lib (even if there's a compatible one),
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.4"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.1", "/lib64/libmpi.so.13.1"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12", "/lib/libmpi.so.12.4",
        })
        .checkFailure();
    // So, we continue the test with MPI dependency libs (same method is used).

    // 2 older 1 equal 2 newer
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.1"})
        .setHostMpiDependencyLibraries({"/lib/libdep.so.4.4"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.1", "/lib/libdep.so.4.6", "/lib/libdep.so.4.4", "/lib/libdep.so.4.3", "/lib64/libdep.so.4.2", "/lib64/libdep.so.5.1"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12", "/lib/libmpi.so.12.1",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12", "/lib64/libmpi.so.12.1",

            "/lib/libdep.so", "/lib/libdep.so.4", "/lib/libdep.so.4.4",
            "/lib64/libdep.so", "/lib64/libdep.so.4", "/lib64/libdep.so.4.4",

            "/lib/libdep.so.4.6",
            "/lib/libdep.so.4.3",
            "/lib64/libdep.so.4.2",
            "/lib64/libdep.so.5.1",
        })
        .expectPreservedPostHookContainerLibraries({
            "/lib/libdep.so.4.6",
            "/lib/libdep.so.4.3",
            "/lib64/libdep.so.4.2",
            "/lib64/libdep.so.5.1",
        })
        .checkSuccessful();

    // 1 equal 2 newer
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.1"})
        .setHostMpiDependencyLibraries({"/lib/libdep.so.4.3"})
        .setPreHookContainerLibraries({{"/lib/libmpi.so.12.1", "/lib64/libdep.so.5.0", "/lib/libdep.so.4.3", "/lib/libdep.so.4.5"}})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12", "/lib/libmpi.so.12.1",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12", "/lib64/libmpi.so.12.1",

            "/lib/libdep.so", "/lib/libdep.so.4", "/lib/libdep.so.4.3",
            "/lib64/libdep.so", "/lib64/libdep.so.4", "/lib64/libdep.so.4.3",

            "/lib64/libdep.so.5.0",
            "/lib/libdep.so.4.5",
        })
        .expectPreservedPostHookContainerLibraries({
            "/lib64/libdep.so.5.0",
            "/lib/libdep.so.4.5",
        })
        .checkSuccessful();

    // 2 newer
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.1"})
        .setHostMpiDependencyLibraries({"/lib/libdep.so.4.2"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.1", "/lib64/libdep.so.4.3", "/lib64/libdep.so.4.5"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12", "/lib/libmpi.so.12.1",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12", "/lib64/libmpi.so.12.1",

            "/lib/libdep.so", "/lib/libdep.so.4", "/lib/libdep.so.4.2",
            "/lib64/libdep.so", "/lib64/libdep.so.4", "/lib64/libdep.so.4.2",

            "/lib64/libdep.so.4.3",
            "/lib64/libdep.so.4.5",
        })
        .expectPreservedPostHookContainerLibraries({
            "/lib64/libdep.so.4.3",
            "/lib64/libdep.so.4.5",
        })
        .checkSuccessful();
}

TEST(MPIHookTestGroup, test_library_injection_preserves_rootlink) {
    // If existing container libs are FULL ABI compatible, libdep.so can be safely overwritten
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.1"})
        .setHostMpiDependencyLibraries({"/lib/libdep.so.4.2"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.1", "/lib64/libdep.so", "/lib64/libdep.so.4.1"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12", "/lib/libmpi.so.12.1",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12", "/lib64/libmpi.so.12.1",

            "/lib/libdep.so", "/lib/libdep.so.4", "/lib/libdep.so.4.2",
            "/lib64/libdep.so", "/lib64/libdep.so.4", "/lib64/libdep.so.4.2",

            "/lib64/libdep.so.4.1",
        })
        .checkSuccessful();

    // If existing container libs are not all FULL ABI compatible (e.g. libdep.so.5),
    // libdep.so will be preserved if it exists in any of the common ld.so paths
    std::vector<boost::filesystem::path> commonPaths = {"/lib", "/lib64", "/usr/lib", "/usr/lib64"};
    // Major Incompatible
    for (auto& p : commonPaths){
        Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.1"})
        .setHostMpiDependencyLibraries({"/lib/libdep.so.4.2"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.1", p/"libdep.so", "/lib64/libdep.so.5"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so",   "/lib/libmpi.so.12",   "/lib/libmpi.so.12.1",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12", "/lib64/libmpi.so.12.1",

            "/lib/libdep.so.4",   "/lib/libdep.so.4.2",
            "/lib64/libdep.so.4", "/lib64/libdep.so.4.2",

            p/"libdep.so",
            "/lib64/libdep.so.5",
        })
        .expectPreservedPostHookContainerLibraries({
            p/"libdep.so",
            "/lib64/libdep.so.5",
        })
        .checkSuccessful();
    }

    // Major-only Compatible
    for (auto& p : commonPaths){
        Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.1"})
        .setHostMpiDependencyLibraries({"/lib/libdep.so.4.2"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.1", p/"libdep.so", "/lib64/libdep.so.4.5"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so",   "/lib/libmpi.so.12",   "/lib/libmpi.so.12.1",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12", "/lib64/libmpi.so.12.1",

            "/lib/libdep.so.4",   "/lib/libdep.so.4.2",
            "/lib64/libdep.so.4", "/lib64/libdep.so.4.2",

            p/"libdep.so",
            "/lib64/libdep.so.4.5",
        })
        .expectPreservedPostHookContainerLibraries({
            p/"libdep.so",
            "/lib64/libdep.so.4.5",
        })
        .checkSuccessful();
    }
}


TEST(MPIHookTestGroup, test_dependency_libraries_injection_container_version_matching) {
    // Reproduces webrt38418
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setHostMpiDependencyLibraries({"/lib/libdep.so.4"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12", "/lib64/libdep.so.3", "/lib64/libdep.so.4"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so",  "/lib/libmpi.so.12",
            "/lib64/libmpi.so","/lib64/libmpi.so.12",

            "/lib/libdep.so",  "/lib/libdep.so.4",
            "/lib64/libdep.so","/lib64/libdep.so.4",

            "/lib64/libdep.so.3",
        })
        .expectPreservedPostHookContainerLibraries({
            "/lib64/libdep.so.3",
        })
        .checkSuccessful();

    // Reproduces webrt38602
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setHostMpiDependencyLibraries({"/lib/libdep.so.4"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12", "/lib64/libdep.so.4", "/lib64/libdep.so.5"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so",   "/lib/libmpi.so.12",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12",

            "/lib/libdep.so",  "/lib/libdep.so.4",
            "/lib64/libdep.so","/lib64/libdep.so.4",

            "/lib64/libdep.so.5",
        })
        .expectPreservedPostHookContainerLibraries({
            "/lib64/libdep.so.5",
        })
        .checkSuccessful();
}

TEST(MPIHookTestGroup, test_bind_mounts) {
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.5.5"})
        .setMpiBindMounts({"/dev/null", "/dev/zero", "/var/opt"})
        .checkSuccessful();
}

TEST(MPIHookTestGroup, test_default_major_abi_compatibility_check) {
    // compatible libraries (same MAJOR, MINOR, PATCH)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.5.5"})
        .checkSuccessful();

    // compatible libraries (same MAJOR, MINOR, compatible PATCH)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.5.0"})
        .checkSuccessful();

    // compatible libraries (same MAJOR, MINOR, compatible PATCH)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.5.10"})
        .checkSuccessful();

    // compatible libraries (same MAJOR, compatible MINOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.4.0"})
        .checkSuccessful();

    // incompatible libraries (same MAJOR, incompatible MINOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.6"})
        .checkSuccessful();

    // incompatible libraries (incompatible MAJOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.11.5.5"})
        .checkFailure();

    // incompatible libraries (incompatible MAJOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.13.5.5"})
        .checkFailure();

    // impossible combatibility check (must have at least MAJOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/lib/libmpi.so"})
        .checkFailure();
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.5.5"})
        .checkFailure();

    // only major available (default MINOR = 0)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.1"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12"})
        .checkSuccessful();
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.0"})
        .checkSuccessful();
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.1"})
        .checkSuccessful();
}

TEST(MPIHookTestGroup, test_full_abi_compatibility_check) {
    // compatible libraries (same MAJOR, MINOR, PATCH)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.5.5"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "full"}})
        .checkSuccessful();

    // compatible libraries (same MAJOR, MINOR, compatible PATCH)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.5.0"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "full"}})
        .checkSuccessful();

    // compatible libraries (same MAJOR, MINOR, compatible PATCH)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.5.10"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "full"}})
        .checkSuccessful();

    // compatible libraries (same MAJOR, compatible MINOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.4.0"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "full"}})
        .checkSuccessful();

    // incompatible libraries (same MAJOR, incompatible MINOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.6"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "full"}})
        .checkSuccessful();

    // incompatible libraries (incompatible MAJOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.11.5.5"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "full"}})
        .checkFailure();

    // incompatible libraries (incompatible MAJOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.13.5.5"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "full"}})
        .checkFailure();

    // impossible combatibility check (must have at least MAJOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/lib/libmpi.so"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "full"}})
        .checkFailure();
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.5.5"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE","full"}})
        .checkFailure();

    // only major available (default MINOR = 0)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.1"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE","full"}})
        .checkSuccessful();
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.0"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE","full"}})
        .checkSuccessful();
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.1"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE","full"}})
        .checkSuccessful();
}

TEST(MPIHookTestGroup, test_strict_abi_compatibility_check) {
    // compatible libraries (same MAJOR, MINOR, PATCH)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.5.5"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "strict"}})
        .checkSuccessful();

    // compatible libraries (same MAJOR, MINOR, compatible PATCH)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.5.0"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "strict"}})
        .checkSuccessful();

    // compatible libraries (same MAJOR, MINOR, compatible PATCH)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.5.10"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "strict"}})
        .checkSuccessful();

    // compatible libraries (same MAJOR, compatible MINOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.4.0"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "strict"}})
        .checkFailure();

    // incompatible libraries (same MAJOR, incompatible MINOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.6"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "strict"}})
        .checkFailure();

    // incompatible libraries (incompatible MAJOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.11.5.5"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "strict"}})
        .checkFailure();

    // incompatible libraries (incompatible MAJOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.13.5.5"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "strict"}})
        .checkFailure();

    // impossible combatibility check (must have at least MAJOR)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/lib/libmpi.so"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE", "strict"}})
        .checkFailure();
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.5.5"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE","strict"}})
        .checkFailure();

    // only major available (default MINOR = 0)
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12.1"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE","strict"}})
        .checkFailure();
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.0"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE","strict"}})
        .checkFailure();
    Checker{}
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.1"})
        .setExtraEnvironmentVariables({{"MPI_COMPATIBILITY_TYPE","strict"}})
        .checkFailure();
}


}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
