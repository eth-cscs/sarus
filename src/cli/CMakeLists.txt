
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB cli_srcs "*.cpp")
add_library(cli_library STATIC ${cli_srcs})
target_link_libraries(cli_library image_manager_library runtime_library ${Boost_LIBRARIES})

if(${ENABLE_UNIT_TESTS})
    add_subdirectory(test)
endif(${ENABLE_UNIT_TESTS})

