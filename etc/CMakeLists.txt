
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/container/nsswitch.conf DESTINATION ${CMAKE_INSTALL_PREFIX}/etc/container)

install(FILES sarus.schema.json hook.schema.json definitions.schema.json DESTINATION ${CMAKE_INSTALL_PREFIX}/etc)
install(FILES policy.json DESTINATION ${CMAKE_INSTALL_PREFIX}/etc)
install(DIRECTORY templates DESTINATION ${CMAKE_INSTALL_PREFIX}/etc)

install(FILES templates/hooks.d/07-ssh-hook.json.in DESTINATION ${CMAKE_INSTALL_PREFIX}/etc/hooks.d)
install(FILES templates/hooks.d/09-slurm-global-sync-hook.json.in DESTINATION ${CMAKE_INSTALL_PREFIX}/etc/hooks.d)
