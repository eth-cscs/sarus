
include(add_unit_test)
set(link_libraries "hooks_glibc_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/hooks/glibc/CMakeFiles/hooks_glibc_library.dir")

add_unit_test(hooks_glibc_GlibcHook_AsRoot test_GlibcHook.cpp "${link_libraries}")
