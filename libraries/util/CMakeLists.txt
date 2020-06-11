file(GLOB HEADERS "include/koinos/*.hpp")
add_library(koinos_util INTERFACE)
target_sources(koinos_util INTERFACE ${HEADERS})
target_include_directories(koinos_util INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
