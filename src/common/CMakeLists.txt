
configure_file(ConfigBuildTime.cpp.in ${CMAKE_CURRENT_SOURCE_DIR}/ConfigBuildTime.cpp)

file(GLOB common_srcs "*.cpp" "*.c")
add_library(common_library STATIC ${common_srcs})
target_link_libraries(common_library ${Boost_LIBRARIES} cpprestsdk::cpprest)

if(${BUILD_STATIC})
    target_link_libraries(common_library pthread dl)
endif(${BUILD_STATIC})

if(${ENABLE_UNIT_TESTS})
    add_subdirectory(test)
endif(${ENABLE_UNIT_TESTS})

