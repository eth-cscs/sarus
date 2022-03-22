
include(add_unit_test)
set(link_libraries "image_manager_library;test_utility_library")
set(object_files_directory "${CMAKE_BINARY_DIR}/src/image_manager/CMakeFiles/image_manager_library.dir")

add_unit_test(image_manager_OCIImage test_OCIImage.cpp "${link_libraries}")
add_unit_test(image_manager_SquashfsImage test_SquashfsImage.cpp "${link_libraries}")
add_unit_test(image_manager_ImageStore test_ImageStore.cpp "${link_libraries}")
add_unit_test(image_manager_SkopeoDriver test_SkopeoDriver.cpp "${link_libraries}")
add_unit_test(image_manager_UmociDriver test_UmociDriver.cpp "${link_libraries}")
add_unit_test(image_manager_Utility test_Utility.cpp "${link_libraries}")
