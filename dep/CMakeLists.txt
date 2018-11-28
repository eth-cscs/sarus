
# CppUTest
set(cpputest_file ${CMAKE_CURRENT_BINARY_DIR}/cpputest/lib/libCppUTest.a)
add_custom_command(OUTPUT ${cpputest_file} COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cpputest.sh)
add_custom_target(cpputest_library DEPENDS ${cpputest_file})
set(CPPUTEST_LIB ${cpputest_file} PARENT_SCOPE)
set(CPPUTEST_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/cpputest/include PARENT_SCOPE)

# static sshd
if(${ENABLE_SSH})
    set(OPENSSH_ARCHIVE ${CMAKE_CURRENT_BINARY_DIR}/openssh.tar CACHE PATH "Archive with the statically-linked openssh")
    add_custom_command(OUTPUT ${OPENSSH_ARCHIVE} COMMAND bash -c \"cd ${CMAKE_CURRENT_BINARY_DIR} && ${CMAKE_CURRENT_SOURCE_DIR}/build_openssh.sh\")
    add_custom_target(openssh_archive ALL DEPENDS ${OPENSSH_ARCHIVE})
    install(CODE "execute_process(COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/openssh)")
    install(CODE "execute_process(COMMAND tar xf ${OPENSSH_ARCHIVE} -C ${CMAKE_INSTALL_PREFIX}/openssh -p --no-same-owner --strip-components=1)")
endif(${ENABLE_SSH})
