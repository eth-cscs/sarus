cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project("sarus")

enable_testing()
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)
set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE 1) # required for gcov (e.g. will generate example.gcno instead of example.cpp.gcno)

# build statically-linked executables
if(${BUILD_STATIC})
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(CMAKE_EXE_LINKER_FLAGS "-static")
    set(CMAKE_LINK_SEARCH_START_STATIC TRUE)
    set(CMAKE_LINK_SEARCH_END_STATIC TRUE)
endif(${BUILD_STATIC})

set(LIBEXECDIR ${CMAKE_INSTALL_PREFIX}/libexec CACHE PATH "libexec directory [$CMAKE_INSTALL_PREFIX/libexec]")
set(ENABLE_SSH TRUE CACHE BOOL "Build a statically-linked SSH software to be executed inside the container [TRUE]")

set(ENABLE_TESTS_WITH_VALGRIND FALSE CACHE BOOL "Run each unit test through valgrind [FALSE]")

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

message(STATUS "Installation directory: " ${CMAKE_INSTALL_PREFIX})
message(STATUS "Enable SSH support: " ${ENABLE_SSH})

find_package(RapidJSON REQUIRED)
include_directories(${RapidJSON_INCLUDE_DIR})

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

find_package(cpprestsdk REQUIRED)
set(cpprestsdk_INCLUDE_DIR CACHE PATH "cpprestsdk's include directory. Specify this directory if cmake cannot find it automatically.")
if(EXISTS ${cpprestsdk_INCLUDE_DIR})
   include_directories(${cpprestsdk_INCLUDE_DIR})
endif()

find_package(LibArchive REQUIRED)
include_directories(${LibArchive_INCLUDE_DIRS})

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

find_package(Boost REQUIRED COMPONENTS program_options system thread filesystem regex random)
include_directories(${Boost_INCLUDE_DIRS})

find_program(GIT_PATH git)

execute_process(COMMAND ${GIT_PATH} -C ${CMAKE_SOURCE_DIR} describe --tags --dirty
                OUTPUT_VARIABLE LATEST_GIT_TAG
                OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT LATEST_GIT_TAG)
    set(LATEST_GIT_TAG VERSION-NOT-AVAILABLE)
endif(NOT LATEST_GIT_TAG)
message(STATUS "Sarus version: " ${LATEST_GIT_TAG})

add_subdirectory(dep)
add_subdirectory(etc)
add_subdirectory(src)

install(FILES sarus.json.in sarus.schema.json hook.schema.json definitions.schema.json DESTINATION ${CMAKE_INSTALL_PREFIX}/etc)
install(FILES 01-ssh-hook.json.in DESTINATION ${CMAKE_INSTALL_PREFIX}/etc/hooks.d)
install(FILES 02-slurm-global-sync-hook.json.in DESTINATION ${CMAKE_INSTALL_PREFIX}/etc/hooks.d)
install(FILES configure_installation.sh DESTINATION ${CMAKE_INSTALL_PREFIX} PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)
