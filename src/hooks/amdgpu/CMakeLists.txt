
file(GLOB hooks_amdgpu_srcs "*.cpp" "*.c")
list(REMOVE_ITEM hooks_amdgpu_srcs ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
add_library(hooks_amdgpu_library STATIC ${hooks_amdgpu_srcs})
target_link_libraries(hooks_amdgpu_library common_library libsarus)

add_executable(amdgpu_hook "main.cpp")
target_link_libraries(amdgpu_hook hooks_amdgpu_library)
install(TARGETS amdgpu_hook DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

if(${ENABLE_UNIT_TESTS})
    add_subdirectory(test)
endif(${ENABLE_UNIT_TESTS})