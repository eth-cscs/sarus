
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/nsswitch.conf DESTINATION ${CMAKE_INSTALL_PREFIX}/files_to_copy_in_container_etc)
install(CODE "execute_process(COMMAND bash -c \"getent passwd >${CMAKE_CURRENT_BINARY_DIR}/passwd\")")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/passwd DESTINATION ${CMAKE_INSTALL_PREFIX}/files_to_copy_in_container_etc)
install(CODE "execute_process(COMMAND bash -c \"getent group >${CMAKE_CURRENT_BINARY_DIR}/group\")")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/group DESTINATION ${CMAKE_INSTALL_PREFIX}/files_to_copy_in_container_etc)
