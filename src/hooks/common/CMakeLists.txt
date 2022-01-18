
file(GLOB hooks_common_srcs "*.cpp" "*.c")
add_library(hooks_common_library STATIC ${hooks_common_srcs})
target_link_libraries(hooks_common_library ${Boost_LIBRARIES} runtime_library)

if(${ENABLE_UNIT_TESTS})
    add_subdirectory(test)
endif(${ENABLE_UNIT_TESTS})
