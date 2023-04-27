
include(add_unit_test)
set(link_libraries "hooks_amdgpu_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/hooks/amdgpu/CMakeFiles/hooks_amdgpu.dir")

add_unit_test(hooks_amdgpu test_AmdGpuHook.cpp "${link_libraries}")
