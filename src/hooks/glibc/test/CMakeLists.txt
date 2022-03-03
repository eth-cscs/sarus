
include(add_unit_test)
set(link_libraries "hooks_glibc_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/hooks/glibc/CMakeFiles/hooks_glibc_library.dir")

add_unit_test(hooks_glibc_GlibcHook_AsRoot test_GlibcHook.cpp "${link_libraries}")

# Link statically only if address sanitizer is not enabled
string(FIND ${CMAKE_CXX_FLAGS} "-fsanitize=address" POS_FOUND)
if(POS_FOUND EQUAL -1)
    link_libraries("-static")
endif()

add_executable(lddMockOlder "lddMockOlder.cpp")
add_executable(lddMockEqual "lddMockEqual.cpp")
add_executable(lddMockNewer "lddMockNewer.cpp")
