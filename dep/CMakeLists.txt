
# CppUTest
set(cpputest_file ${CMAKE_CURRENT_BINARY_DIR}/cpputest/lib/libCppUTest.a)
add_custom_command(OUTPUT ${cpputest_file} COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cpputest.sh)
add_custom_target(cpputest_library DEPENDS ${cpputest_file})
set(CPPUTEST_LIB ${cpputest_file} PARENT_SCOPE)
set(CPPUTEST_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/cpputest/include PARENT_SCOPE)

# static ssh binary
if(${ENABLE_SSH})
    set(DROPBEARMULTI_PATH ${CMAKE_CURRENT_BINARY_DIR}/dropbearmulti CACHE PATH "Dropbear single executable a la busybox")
    add_custom_command(OUTPUT ${DROPBEARMULTI_PATH} COMMAND bash -c \"cd ${CMAKE_CURRENT_BINARY_DIR} && ${CMAKE_CURRENT_SOURCE_DIR}/build_dropbear.sh\")
    add_custom_target(dropbear ALL DEPENDS ${DROPBEARMULTI_PATH})
    install(FILES ${DROPBEARMULTI_PATH} DESTINATION ${CMAKE_INSTALL_PREFIX}/dropbear/bin PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink dropbearmulti ${CMAKE_INSTALL_PREFIX}/dropbear/bin/dbclient)")
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink dropbearmulti ${CMAKE_INSTALL_PREFIX}/dropbear/bin/dropbear)")
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink dropbearmulti ${CMAKE_INSTALL_PREFIX}/dropbear/bin/dropbearkey)")
endif(${ENABLE_SSH})
