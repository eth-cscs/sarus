
include(add_unit_test)
set(link_libraries "hooks_mpi_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/hooks/mpi/CMakeFiles/hooks_mpi_library.dir")

add_unit_test(hooks_mpi_MPIHook_AsRoot test_MPIHook.cpp "${link_libraries}")
add_unit_test(hooks_mpi_SharedLibrary test_SharedLibrary.cpp "${link_libraries}")
add_unit_test(hooks_mpi_AbiChecker test_AbiChecker.cpp "${link_libraries}")
