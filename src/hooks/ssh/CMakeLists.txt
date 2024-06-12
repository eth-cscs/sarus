
file(GLOB hook_srcs "*.cpp" "*.c")
list(REMOVE_ITEM hook_srcs ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
add_library(hooks_ssh_library STATIC ${hook_srcs})
target_link_libraries(hooks_ssh_library runtime_library common_library)

add_executable(ssh_hook "main.cpp")
target_link_libraries(ssh_hook hooks_ssh_library)
install(TARGETS ssh_hook DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

if(${ENABLE_UNIT_TESTS})
    add_subdirectory(test)
endif(${ENABLE_UNIT_TESTS})

