
include(add_unit_test)
set(link_libraries "image_manager_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/image_manager/CMakeFiles/image_manager_library.dir")

add_unit_test(test_image_manager_LoadedImage test_LoadedImage.cpp LoadedImage.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(test_image_manager_PulledImage test_PulledImage.cpp PulledImage.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(test_image_manager_SquashfsImage test_SquashfsImage.cpp SquashfsImage.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(test_image_manager_ImageStore test_ImageStore.cpp ImageStore.cpp "${link_libraries}" ${object_files_directory})
add_unit_test(test_image_manager_Puller test_Puller.cpp Puller.cpp "${link_libraries}" ${object_files_directory})
