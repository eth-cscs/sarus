
include(add_unit_test)
set(link_libraries "cli_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/cli/CMakeFiles/cli_library.dir")

add_unit_test(test_common_Utility_AsRoot test_Utility.cpp Utility.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(test_common_Logger test_Logger.cpp Logger.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(test_common_Error test_Error.cpp Error.hpp "${link_libraries}" ${object_files_directory})
add_unit_test(test_common_ImageMetadata test_ImageMetadata.cpp ImageMetadata.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(test_common_Lockfile test_Lockfile.cpp Lockfile.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(test_common_JSON test_JSON.cpp NO_UUT_SRC_FILE "${link_libraries}" ${object_files_directory})
add_unit_test(test_common_CLIArguments test_CLIArguments.cpp CLIArguments.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(test_common_SecurityChecks_AsRoot test_SecurityChecks.cpp SecurityChecks.cpp "${link_libraries}" ${object_files_directory})
