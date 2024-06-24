
add_executable(stdout_stderr_test_hook "main.cpp")
target_link_libraries(stdout_stderr_test_hook common_library libsarus)
install(TARGETS stdout_stderr_test_hook DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)
