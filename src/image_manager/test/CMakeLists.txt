
include(add_unit_test)
set(link_libraries "image_manager_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/image_manager/CMakeFiles/image_manager_library.dir")

add_unit_test(image_manager_InputImage test_InputImage.cpp "${link_libraries}")
add_unit_test(image_manager_LoadedImage test_LoadedImage.cpp "${link_libraries}")
add_unit_test(image_manager_PulledImage test_PulledImage.cpp "${link_libraries}")
add_unit_test(image_manager_SquashfsImage test_SquashfsImage.cpp "${link_libraries}")
add_unit_test(image_manager_ImageStore test_ImageStore.cpp "${link_libraries}")
add_unit_test(image_manager_Puller test_Puller.cpp "${link_libraries}")
