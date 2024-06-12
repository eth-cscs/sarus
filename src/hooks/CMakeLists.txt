
add_subdirectory(amdgpu)
add_subdirectory(glibc)
add_subdirectory(mount)
add_subdirectory(mpi)
add_subdirectory(ssh)
add_subdirectory(slurm_global_sync)
add_subdirectory(timestamp)
add_subdirectory(stdout_stderr_test)

# Collective target for hooks
add_custom_target(hooks ALL)
add_dependencies(hooks glibc_hook mount_hook mpi_hook slurm_global_sync_hook ssh_hook stdout_stderr_test_hook timestamp_hook)
