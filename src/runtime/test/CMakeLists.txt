
include(add_unit_test)
set(link_libraries "runtime_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/runtime/CMakeFiles/runtime_library.dir")

add_unit_test_as_root(runtime_Runtime test_Runtime.cpp "${link_libraries}")
add_unit_test(runtime_OCIHooks test_OCIHooks.cpp "${link_libraries}")
add_unit_test(runtime_OCIBundleConfig test_OCIBundleConfig.cpp "${link_libraries}")
add_unit_test_as_root(runtime_OCIBundleConfig test_OCIBundleConfig.cpp "${link_libraries}")
add_unit_test(runtime_ConfigsMerger test_ConfigsMerger.cpp "${link_libraries}")
add_unit_test(runtime_FileDescriptorHandler test_FileDescriptorHandler.cpp "${link_libraries}")
add_unit_test_as_root(runtime_SecurityChecks test_SecurityChecks.cpp "${link_libraries}")
