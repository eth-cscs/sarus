
include(add_unit_test)
set(link_libraries "cli_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/common/CMakeFiles/common_library.dir")

add_unit_test(test_common_Utility_AsRoot test_Utility.cpp "${link_libraries}")
add_unit_test(test_common_Logger test_Logger.cpp "${link_libraries}")
add_unit_test(test_common_Error test_Error.cpp "${link_libraries}")
add_unit_test(test_common_ImageMetadata test_ImageMetadata.cpp "${link_libraries}")
add_unit_test(test_common_Lockfile test_Lockfile.cpp "${link_libraries}")
add_unit_test(test_common_JSON test_JSON.cpp "${link_libraries}")
add_unit_test(test_common_CLIArguments test_CLIArguments.cpp "${link_libraries}")
add_unit_test(test_common_PasswdDB test_PasswdDB.cpp "${link_libraries}")
add_unit_test(test_common_GroupDB test_GroupDB.cpp "${link_libraries}")
