
include(add_unit_test)
set(link_libraries "cli_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/cli/CMakeFiles/cli_library.dir")

add_unit_test(cli_CLI test_CLI.cpp "${link_libraries}")
add_unit_test_as_root(cli_CLI test_CLI.cpp "${link_libraries}")
add_unit_test(cli_Utility test_Utility.cpp "${link_libraries}")
add_unit_test(cli_MountParser test_MountParser.cpp "${link_libraries}")
add_unit_test_as_root(cli_DeviceParser test_DeviceParser.cpp "${link_libraries}")
