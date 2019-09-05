
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_definitions(-DBOOST_NO_CXX11_SCOPED_ENUMS) # workaround (problem linking boost::filesystem::detail::copy_file)

# sarus executable
add_executable(sarus "main.cpp")
target_link_libraries(sarus cli_library common_library)
install(TARGETS sarus DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    SETUID OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

add_subdirectory(common)
add_subdirectory(cli)
add_subdirectory(image_manager)
add_subdirectory(runtime)
add_subdirectory(test_utility)
add_subdirectory(hooks)
