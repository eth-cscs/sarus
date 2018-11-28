
file(GLOB runtime_srcs "*.cpp" "*.c")
add_library(runtime_library STATIC ${runtime_srcs})
target_link_libraries(runtime_library common_library)

add_subdirectory(test)
