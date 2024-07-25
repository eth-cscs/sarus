file(GLOB libsarus_testaux_srcs "*.cpp")
add_library(libsarus_testaux STATIC ${libsarus_testaux_srcs})
target_link_libraries(libsarus_testaux ${Boost_LIBRARIES} libsarus)
set_target_properties(libsarus_testaux PROPERTIES PREFIX "")
