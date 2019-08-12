
include(add_unit_test)
set(link_libraries "hooks_timestamp_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/hooks/timestamp/CMakeFiles/hooks_timestamp_library.dir")

add_unit_test(hooks_timestamp_TimestampHook_AsRoot test_TimestampHook.cpp "${link_libraries}")