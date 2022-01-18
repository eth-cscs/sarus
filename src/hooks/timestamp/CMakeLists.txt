
file(GLOB hooks_timestamp_srcs "*.cpp" "*.c")
list(REMOVE_ITEM hooks_timestamp_srcs ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
add_library(hooks_timestamp_library STATIC ${hooks_timestamp_srcs})
target_link_libraries(hooks_timestamp_library runtime_library hooks_common_library common_library)

add_executable(timestamp_hook "main.cpp")
target_link_libraries(timestamp_hook hooks_timestamp_library)
install(TARGETS timestamp_hook DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

if(${ENABLE_UNIT_TESTS})
    add_subdirectory(test)
endif(${ENABLE_UNIT_TESTS})

