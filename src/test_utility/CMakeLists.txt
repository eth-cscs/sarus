
file(GLOB test_utility_srcs "*.cpp")
add_library(test_utility_library STATIC ${test_utility_srcs})
target_link_libraries(test_utility_library ${Boost_LIBRARIES} common_library)
