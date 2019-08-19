
if(${ENABLE_SSH})
    include(add_unit_test)
    set(link_libraries "hooks_ssh_library;test_utility_library")
    set(object_files_directory "${CMAKE_BINARY_DIR}/src/hooks/ssh/CMakeFiles/hooks_ssh_library.dir")

    add_unit_test(hooks_ssh_SSHHook_AsRoot test_SSHHook.cpp "${link_libraries}")
endif(${ENABLE_SSH})
