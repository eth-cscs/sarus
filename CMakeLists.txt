cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project("sarus")

enable_testing()
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)
set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE 1) # required for gcov (e.g. will generate example.gcno instead of example.cpp.gcno)

set(SYSCONFDIR ${CMAKE_INSTALL_PREFIX}/etc CACHE PATH "Configuration files directory [$CMAKE_INSTALL_PREFIX/etc]")
set(LIBEXECDIR ${CMAKE_INSTALL_PREFIX}/libexec CACHE PATH "libexec directory [$CMAKE_INSTALL_PREFIX/libexec]")
set(DIR_OF_FILES_TO_COPY_IN_CONTAINER_ETC ${CMAKE_INSTALL_PREFIX}/files_to_copy_in_container_etc CACHE PATH "Directory of configuration files copied into the container [$CMAKE_INSTALL_PREFIX/files_to_copy_in_container_etc]")
set(ENABLE_RUNTIME_SECURITY_CHECKS TRUE CACHE BOOL "Enable runtime security checks (root ownership of files, etc.) [TRUE]")
set(ENABLE_SSH TRUE CACHE BOOL "Build a statically-linked ssh to be executed inside the container [TRUE]")

set(ENABLE_TESTS_WITH_GCOV FALSE CACHE BOOL "Run gcov after each unit test to collect coverage information [FALSE]")
set(ENABLE_TESTS_WITH_VALGRIND FALSE CACHE BOOL "Run each unit test through valgrind [FALSE]")

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

message(STATUS "Installation directory: " ${CMAKE_INSTALL_PREFIX})
message(STATUS "Enable runtime security checks: " ${ENABLE_RUNTIME_SECURITY_CHECKS})
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
find_program(MKSQUASHFS_PATH mksquashfs)
find_program(MKFSXFS_PATH mkfs.xfs)
find_program(RUNC_PATH runc)
find_program(RUNC_PATH runc.amd64)

execute_process(COMMAND ${GIT_PATH} describe --tags --dirty
                OUTPUT_VARIABLE LATEST_GIT_TAG
                OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT LATEST_GIT_TAG)
    file(STRINGS VERSION VERSION_STRINGS)
    list(GET VERSION_STRINGS 0 LATEST_GIT_TAG)
endif(NOT LATEST_GIT_TAG)
message(STATUS "Sarus version: " ${LATEST_GIT_TAG})

add_subdirectory(dep)
add_subdirectory(files_to_copy_in_container_etc)
add_subdirectory(src)

configure_file(sarus.json.in ${CMAKE_BINARY_DIR}/sarus.json)
install(FILES ${CMAKE_BINARY_DIR}/sarus.json sarus.schema.json DESTINATION ${SYSCONFDIR})
