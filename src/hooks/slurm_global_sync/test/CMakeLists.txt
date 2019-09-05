
include(add_unit_test)
set(link_libraries "hooks_slurm_global_sync_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/hooks/slurm_global_sync/CMakeFiles/hooks_slurm_global_sync_library.dir")

add_unit_test(hooks_slurm_global_sync_Hook_AsRoot test_hook.cpp "${link_libraries}")
