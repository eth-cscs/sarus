
include(add_unit_test)
set(link_libraries "cli_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/common/CMakeFiles/common_library.dir")

add_unit_test(common_ImageMetadata test_ImageMetadata.cpp "${link_libraries}")
add_unit_test(common_ImageReference test_ImageReference.cpp "${link_libraries}")
add_unit_test(common_JSON test_JSON.cpp "${link_libraries}")
add_unit_test(common_GroupDB test_GroupDB.cpp "${link_libraries}")
