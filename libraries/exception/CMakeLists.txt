file(GLOB HEADERS "include/koinos/*.hpp")
add_library(koinos_exception ${HEADERS} exception.cpp)
target_link_libraries( koinos_exception koinos_types koinos_log nlohmann_json )
target_include_directories(koinos_exception PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
