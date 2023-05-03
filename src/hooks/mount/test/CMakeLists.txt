
include(add_unit_test)
set(link_libraries "hooks_mount_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/hooks/mount/CMakeFiles/hooks_mount_library.dir")

add_unit_test(hooks_mount_MountHook_AsRoot test_MountHook.cpp "${link_libraries}")
