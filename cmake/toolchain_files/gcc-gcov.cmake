
include("${CMAKE_CURRENT_LIST_DIR}/gcc.cmake")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
