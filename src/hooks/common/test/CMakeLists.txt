
include(add_unit_test)
set(link_libraries "hooks_common_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/hooks/common/CMakeFiles/hooks_common_library.dir")

add_unit_test(hooks_common_Utility_AsRoot test_Utility.cpp "${link_libraries}")