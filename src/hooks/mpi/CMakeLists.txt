
file(GLOB hooks_mpi_srcs "*.cpp" "*.c")
list(REMOVE_ITEM hooks_mpi_srcs ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
add_library(hooks_mpi_library STATIC ${hooks_mpi_srcs})
target_link_libraries(hooks_mpi_library runtime_library hooks_common_library common_library)

add_executable(mpi_hook "main.cpp")
target_link_libraries(mpi_hook hooks_mpi_library)
install(TARGETS mpi_hook DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

add_subdirectory(test)