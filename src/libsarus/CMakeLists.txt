file(GLOB libsarus_srcs "*.cpp" "utility/*.cpp")
add_library(libsarus STATIC ${libsarus_srcs})
target_link_libraries(libsarus ${Boost_LIBRARIES})
set_target_properties(libsarus PROPERTIES PREFIX "")

if(${BUILD_STATIC})
  target_link_libraries(libsarus pthread dl)
endif(${BUILD_STATIC})

if(${ENABLE_UNIT_TESTS})
  add_subdirectory(test)
endif(${ENABLE_UNIT_TESTS})
