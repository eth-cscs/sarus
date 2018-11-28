
include(add_unit_test)
set(link_libraries "cli_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/cli/CMakeFiles/cli_library.dir")

add_unit_test(cli_CLI test_CLI.cpp CLI.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(cli_Utility test_Utility.cpp Utility.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(cli_MountParser test_MountParser.cpp MountParser.cpp "${link_libraries}" ${object_files_directory})
