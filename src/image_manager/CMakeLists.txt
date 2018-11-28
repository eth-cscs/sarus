
file(GLOB image_manager_library_srcs "*.cpp")
add_library(image_manager_library STATIC ${image_manager_library_srcs})
target_link_libraries(image_manager_library common_library ${ZLIB_LIBRARIES} ${Boost_LIBRARIES} ${LibArchive_LIBRARIES} cpprestsdk::cpprest ${OPENSSL_LIBRARIES})

add_subdirectory(test)
