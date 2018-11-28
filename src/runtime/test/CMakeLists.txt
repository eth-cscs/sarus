
include(add_unit_test)
set(link_libraries "runtime_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/runtime/CMakeFiles/runtime_library.dir")

add_unit_test(runtime_MountUtilities_AsRoot test_mount_utilities.cpp mount_utilities.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(runtime_SiteMount_AsRoot test_SiteMount.cpp SiteMount.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(runtime_UserMount_AsRoot test_UserMount.cpp UserMount.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(runtime_Runtime_AsRoot test_Runtime.cpp Runtime.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(runtime_OCIBundleConfig test_OCIBundleConfig.cpp OCIBundleConfig.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(runtime_ConfigsMerger test_ConfigsMerger.cpp ConfigsMerger.cpp "${link_libraries}" ${object_files_directory})
