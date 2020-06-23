
include(add_unit_test)
set(link_libraries "runtime_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/runtime/CMakeFiles/runtime_library.dir")

add_unit_test(runtime_MountUtilities_AsRoot test_mount_utilities.cpp "${link_libraries}")
add_unit_test(runtime_Mount_AsRoot test_Mount.cpp "${link_libraries}")
add_unit_test(runtime_Runtime_AsRoot test_Runtime.cpp "${link_libraries}")
add_unit_test(runtime_OCIHooks test_OCIHooks.cpp "${link_libraries}")
add_unit_test(runtime_OCIBundleConfig test_OCIBundleConfig.cpp "${link_libraries}")
add_unit_test(runtime_ConfigsMerger test_ConfigsMerger.cpp "${link_libraries}")
add_unit_test(runtime_FileDescriptorHandler test_FileDescriptorHandler.cpp "${link_libraries}")
add_unit_test(runtime_SecurityChecks_AsRoot test_SecurityChecks.cpp "${link_libraries}")
