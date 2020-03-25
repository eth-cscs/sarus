/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/Logger.hpp"
#include "Checker.hpp"
#include "test_utility/unittest_main_function.hpp"


namespace sarus {
namespace hooks {
namespace mpi {
namespace test {


TEST_GROUP(MPIHookTestGroup) {
};

TEST(MPIHookTestGroup, test_basics) {
    // MPI hook disabled
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "false"}})
        .setHostMpiLibraries({})
        .setPreHookContainerLibraries({})
        .checkSuccessfull();

    // no MPI libraries in host
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({})
        .setPreHookContainerLibraries({})
        .checkFailure();

    // no MPI libraries in container
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({})
        .checkFailure();
}

TEST(MPIHookTestGroup, test_mpi_libraries_injection) {
    // MPI library in non-default linker directory
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/local/lib/libmpi.so.12.5.5"})
        .expectPostHookContainerLibraries({
            "/usr/local/lib/libmpi.so.12.5.5",
            "/lib/libmpi.so", "/lib/libmpi.so.12", "/lib/libmpi.so.12.5", "/lib/libmpi.so.12.5.5",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12", "/lib64/libmpi.so.12.5", "/lib64/libmpi.so.12.5.5"})
        .checkSuccessfull();

    // multiple libraries
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5", "/lib/libmpicxx.so.12.5.5"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.5.5", "/lib/libmpicxx.so.12.5.5"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12", "/lib/libmpi.so.12.5", "/lib/libmpi.so.12.5.5",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12", "/lib64/libmpi.so.12.5", "/lib64/libmpi.so.12.5.5",

            "/lib/libmpicxx.so", "/lib/libmpicxx.so.12", "/lib/libmpicxx.so.12.5", "/lib/libmpicxx.so.12.5.5",
            "/lib64/libmpicxx.so", "/lib64/libmpicxx.so.12", "/lib64/libmpicxx.so.12.5", "/lib64/libmpicxx.so.12.5.5"})
        .checkSuccessfull();

    // multiple libraries (not all in container, but all injected)
    // Note: we inject all the host MPI libraries also when they are not present in the container because we don't
    // know about the dependencies between the host's MPI libraries. E.g. libmpicxx.so might depend on libmpi.so
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5", "/lib/libmpicxx.so.12.5.5"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.5.5"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12", "/lib/libmpi.so.12.5", "/lib/libmpi.so.12.5.5",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12", "/lib64/libmpi.so.12.5", "/lib64/libmpi.so.12.5.5",

            "/lib/libmpicxx.so", "/lib/libmpicxx.so.12", "/lib/libmpicxx.so.12.5", "/lib/libmpicxx.so.12.5.5",
            "/lib64/libmpicxx.so", "/lib64/libmpicxx.so.12", "/lib64/libmpicxx.so.12.5", "/lib64/libmpicxx.so.12.5.5"})
        .checkSuccessfull();
}

TEST(MPIHookTestGroup, test_dependency_libraries_injection) {
    // no libdep.so in container => create it
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setHostMpiDependencyLibraries({"/lib/libdep.so"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12",

            "/lib/libdep.so",
            "/lib64/libdep.so"})
        .checkSuccessfull();

    // container's libdep.so gets replaced with host's library
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setHostMpiDependencyLibraries({"/lib/libdep.so"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12", "/usr/local/lib/libdep.so"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12",

            "/usr/local/lib/libdep.so",
            "/lib/libdep.so",
            "/lib64/libdep.so"})
        .checkSuccessfull();

    // multiple libraries (libdep0.so, libdep1.so)
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
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
        .checkSuccessfull();

    // symlinks already exist (are replaced by the hook)
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setHostMpiDependencyLibraries({"/lib/libdep.so"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12", "/lib/libdep.so", "/lib64/libdep.so"})
        .expectPostHookContainerLibraries({
            "/lib/libmpi.so", "/lib/libmpi.so.12",
            "/lib64/libmpi.so", "/lib64/libmpi.so.12",

            "/lib/libdep.so",
            "/lib64/libdep.so"})
        .checkSuccessfull();
}

TEST(MPIHookTestGroup, test_bind_mounts) {
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.5.5"})
        .setMpiBindMounts({"/dev/null", "/dev/zero"})
        .checkSuccessfull();
}

TEST(MPIHookTestGroup, test_abi_compatibility_check) {
    // compatible libraries (same MAJOR, MINOR, PATCH)
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.5.5"})
        .checkSuccessfull();

    // compatible libraries (same MAJOR, MINOR, compatible PATCH)
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.5.0"})
        .checkSuccessfull();

    // compatible libraries (same MAJOR, MINOR, compatible PATCH)
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.5.10"})
        .checkSuccessfull();

    // compatible libraries (same MAJOR, compatible MINOR)
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.4.0"})
        .checkSuccessfull();

    // incompatible libraries (same MAJOR, incompatible MINOR)
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.6"})
        .checkSuccessfull();

    // incompatible libraries (incompatible MAJOR)
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.11.5.5"})
        .checkFailure();

    // incompatible libraries (incompatible MAJOR)
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.13.5.5"})
        .checkFailure();

    // impossible combatibility check (must have at least MAJOR)
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12.5.5"})
        .setPreHookContainerLibraries({"/lib/libmpi.so"})
        .checkFailure();
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so"})
        .setPreHookContainerLibraries({"/lib/libmpi.so.12.5.5"})
        .checkFailure();

    // only major available (default MINOR = 0)
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12.1"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12"})
        .checkSuccessfull();
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.0"})
        .checkSuccessfull();
    Checker{}
        .setAnnotationsInConfigJSON({{"com.hooks.mpi.enabled", "true"}})
        .setHostMpiLibraries({"/lib/libmpi.so.12"})
        .setPreHookContainerLibraries({"/usr/lib/libmpi.so.12.1"})
        .checkSuccessfull();
}

}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
