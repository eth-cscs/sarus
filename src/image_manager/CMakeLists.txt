
file(GLOB image_manager_library_srcs "*.cpp")
add_library(image_manager_library STATIC ${image_manager_library_srcs})
target_link_libraries(image_manager_library common_library ${Boost_LIBRARIES})

if(${ENABLE_UNIT_TESTS})
    add_subdirectory(test)
endif(${ENABLE_UNIT_TESTS})
