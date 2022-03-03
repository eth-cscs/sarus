# Disable sanitizing for static helper binaries mocking ldd
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-sanitize=all")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-sanitize=all")
link_libraries("-static")

add_executable(lddMockOlder "lddMockOlder.cpp")
add_executable(lddMockEqual "lddMockEqual.cpp")
add_executable(lddMockNewer "lddMockNewer.cpp")
