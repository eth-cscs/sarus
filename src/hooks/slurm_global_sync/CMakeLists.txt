
file(GLOB hook_srcs "*.cpp" "*.c")
list(REMOVE_ITEM hook_srcs ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
add_library(hooks_slurm_global_sync_library STATIC ${hook_srcs})
target_link_libraries(hooks_slurm_global_sync_library common_library)

add_executable(slurm_global_sync_hook "main.cpp")
target_link_libraries(slurm_global_sync_hook hooks_slurm_global_sync_library)
install(TARGETS slurm_global_sync_hook DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

if(${ENABLE_UNIT_TESTS})
    add_subdirectory(test)
endif(${ENABLE_UNIT_TESTS})
