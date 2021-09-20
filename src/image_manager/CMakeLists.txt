
find_package(EXPAT REQUIRED)

file(GLOB image_manager_library_srcs "*.cpp")
add_library(image_manager_library STATIC ${image_manager_library_srcs})
target_link_libraries(image_manager_library common_library cpprestsdk::cpprest ${LibArchive_LIBRARIES} ${EXPAT_LIBRARIES} ${ZLIB_LIBRARIES} ${Boost_LIBRARIES} ${OPENSSL_CRYPTO_LIBRARIES})

add_subdirectory(test)
