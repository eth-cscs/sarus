
file(GLOB hooks_glibc_srcs "*.cpp" "*.c")
list(REMOVE_ITEM hooks_glibc_srcs ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
add_library(hooks_glibc_library STATIC ${hooks_glibc_srcs})
target_link_libraries(hooks_glibc_library common_library runtime_library hooks_common_library)

add_executable(glibc_hook "main.cpp")
target_link_libraries(glibc_hook hooks_glibc_library)
install(TARGETS glibc_hook DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

add_subdirectory(test)

