
SET(CMAKE_CXX_FLAGS "-Wall -std=c++11 -fPIC -fstack-protector-strong -fdata-sections -ffunction-sections -Wl,--gc-sections" CACHE STRING "Common flags for C++ compiler")
SET(CMAKE_CXX_FLAGS_DEBUG "-g -O0" CACHE STRING "Debug flags for C++ compiler")
SET(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3" CACHE STRING "Release flags for C++ compiler")
SET(CMAKE_CXX_FLAGS_MINSIZEREL "-DNDEBUG -Os" CACHE STRING "Minimum size release flags for C++ compiler")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-DNDEBUG -Og -g" CACHE STRING "Release with debug info flags for C++ compiler")

# use -fno-strict-aliasing because C programmers of third-party software might rely on such flag
# (C programmers often rely on it)
SET(CMAKE_C_FLAGS "-Wall -fno-strict-aliasing -fPIC -fstack-protector-strong -fdata-sections -ffunction-sections -Wl,--gc-sections" CACHE STRING "Common flags for C compiler")
SET(CMAKE_C_FLAGS_DEBUG "-g -O0" CACHE STRING "Debug flags for C compiler")
SET(CMAKE_C_FLAGS_RELEASE "-DNDEBUG -O3" CACHE STRING "Release flags for C compiler")
SET(CMAKE_C_FLAGS_MINSIZEREL "-DNDEBUG -Os" CACHE STRING "Minimum size release flags for C compiler")
SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-DNDEBUG -Og -g" CACHE STRING "Release with debug info flags for C compiler")

SET(CMAKE_EXE_LINKER_FLAGS "" CACHE STRING "General flags for linker")

