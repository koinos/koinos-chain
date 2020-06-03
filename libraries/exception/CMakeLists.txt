file(GLOB HEADERS "include/koinos/*.hpp")
add_library(koinos_exception ${HEADERS} exception.cpp)
target_link_libraries( koinos_exception strpolate )
target_include_directories(koinos_exception PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
