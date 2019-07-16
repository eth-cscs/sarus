
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_definitions(-DBOOST_NO_CXX11_SCOPED_ENUMS) # workaround (problem linking boost::filesystem::detail::copy_file)

if(${ENABLE_TESTS_WITH_GCOV})
    set(GCC_COVERAGE_COMPILE_FLAGS "--coverage")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")
endif(${ENABLE_TESTS_WITH_GCOV})

# sarus executable
add_executable(sarus "main.cpp")

if(${BUILD_STATIC})
    list(APPEND Boost_LIBRARIES libdl.a)
    target_link_libraries(sarus -static)
    set_target_properties(sarus PROPERTIES LINK_SEARCH_START_STATIC 1 LINK_SEARCH_END_STATIC 1)
endif(${BUILD_STATIC})

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
