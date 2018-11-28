#include "common/Logger.hpp"
#include "Checker.hpp"
#include "test_utility/unittest_main_function.hpp"


namespace sarus {
namespace hooks {
namespace mpi {
namespace test {


TEST_GROUP(MPIHookTestGroup) {
};

TEST(MPIHookTestGroup, test) {
    // MPI hook disabled
    Checker{}
        .setEnvironmentVariablesInConfigJSON({"SARUS_MPI_HOOK=0"})
        .setHostMpiLibraries({})
        .setContainerMpiLibraries({})
        .checkSuccessfull();

    // no MPI libraries in host
    Checker{}
        .setEnvironmentVariablesInConfigJSON({"SARUS_MPI_HOOK=1"})
        .setHostMpiLibraries({})
        .setContainerMpiLibraries({})
        .checkFailure();

    // no MPI libraries in container
    Checker{}
        .setEnvironmentVariablesInConfigJSON({"SARUS_MPI_HOOK=1"})
        .setHostMpiLibraries({"libmpi.so.12.5.5"})
        .setContainerMpiLibraries({})
        .checkFailure();

    // compatible libraries (same MAJOR, MINOR, PATCH)
    Checker{}
        .setEnvironmentVariablesInConfigJSON({"SARUS_MPI_HOOK=1"})
        .setHostMpiLibraries({"libmpi.so.12.5.5"})
        .setContainerMpiLibraries({"libmpi.so.12.5.5"})
        .setHostMpiDependencyLibraries({"libdependency0.so", "libdependency1.so"})
        .setMpiBindMounts({"/dev/null", "/dev/zero"})
        .checkSuccessfull();

    // compatible libraries (same MAJOR, MINOR, PATCH) in non-default directory
    Checker{}
        .setEnvironmentVariablesInConfigJSON({"SARUS_MPI_HOOK=1"})
        .setHostMpiLibraries({"libmpi.so.12.5.5"})
        .setNonDefaultContainerMpiLibraries({"usr/local/lib/libmpi.so.12.5.5"})
        .setHostMpiDependencyLibraries({"libdependency0.so", "libdependency1.so"})
        .setMpiBindMounts({"/dev/null", "/dev/zero"})
        .setupDefaultLinkerDir({"usr/lib"})
        .checkSuccessfull();

    // compatible libraries (same MAJOR, MINOR, PATCH) in non-default 64-bit directory
    Checker{}
        .setEnvironmentVariablesInConfigJSON({"SARUS_MPI_HOOK=1"})
        .setHostMpiLibraries({"libmpi.so.12.5.5"})
        .setNonDefaultContainerMpiLibraries({"usr/local/lib/libmpi.so.12.5.5"})
        .setHostMpiDependencyLibraries({"libdependency0.so", "libdependency1.so"})
        .setMpiBindMounts({"/dev/null", "/dev/zero"})
        .setupDefaultLinkerDir({"usr/lib64"})
        .checkSuccessfull();

    // compatible libraries (same MAJOR, MINOR, compatible PATCH)
    Checker{}
        .setEnvironmentVariablesInConfigJSON({"SARUS_MPI_HOOK=1"})
        .setHostMpiLibraries({"libmpi.so.12.5.5"})
        .setContainerMpiLibraries({"libmpi.so.12.5.0"})
        .setHostMpiDependencyLibraries({"libdependency0.so", "libdependency1.so"})
        .setMpiBindMounts({"/dev/null", "/dev/zero"})
        .checkSuccessfull();

    // compatible libraries (same MAJOR, MINOR, compatible PATCH)
    Checker{}
        .setEnvironmentVariablesInConfigJSON({"SARUS_MPI_HOOK=1"})
        .setHostMpiLibraries({"libmpi.so.12.5.5"})
        .setContainerMpiLibraries({"libmpi.so.12.5.10"})
        .setHostMpiDependencyLibraries({"libdependency0.so", "libdependency1.so"})
        .setMpiBindMounts({"/dev/null", "/dev/zero"})
        .checkSuccessfull();

    // compatible libraries (same MAJOR, compatible MINOR)
    Checker{}
        .setEnvironmentVariablesInConfigJSON({"SARUS_MPI_HOOK=1"})
        .setHostMpiLibraries({"libmpi.so.12.5.5"})
        .setContainerMpiLibraries({"libmpi.so.12.4"})
        .setHostMpiDependencyLibraries({"libdependency0.so", "libdependency1.so"})
        .setMpiBindMounts({"/dev/null", "/dev/zero"})
        .checkSuccessfull();

    // incompatible libraries (same MAJOR, incompatible MINOR)
    Checker{}
        .setEnvironmentVariablesInConfigJSON({"SARUS_MPI_HOOK=1"})
        .setHostMpiLibraries({"libmpi.so.12.5.5"})
        .setContainerMpiLibraries({"libmpi.so.12.6"})
        .setHostMpiDependencyLibraries({"libdependency0.so", "libdependency1.so"})
        .setMpiBindMounts({"/dev/null", "/dev/zero"})
        .checkFailure();

    // incompatible libraries (incompatible MAJOR)
    Checker{}
        .setEnvironmentVariablesInConfigJSON({"SARUS_MPI_HOOK=1"})
        .setHostMpiLibraries({"libmpi.so.12.5.5"})
        .setContainerMpiLibraries({"libmpi.so.11.5.5"})
        .setHostMpiDependencyLibraries({"libdependency0.so", "libdependency1.so"})
        .setMpiBindMounts({"/dev/null", "/dev/zero"})
        .checkFailure();

    // incompatible libraries (incompatible MAJOR)
    Checker{}
        .setEnvironmentVariablesInConfigJSON({"SARUS_MPI_HOOK=1"})
        .setHostMpiLibraries({"libmpi.so.12.5.5"})
        .setContainerMpiLibraries({"libmpi.so.13.5.5"})
        .setHostMpiDependencyLibraries({"libdependency0.so", "libdependency1.so"})
        .setMpiBindMounts({"/dev/null", "/dev/zero"})
        .checkFailure();
}

}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
