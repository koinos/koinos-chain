file(GLOB HEADERS "include/koinos/statedb/*.hpp" "include/koinos/statedb/detail/*.hpp")
add_library(koinos_statedb ${HEADERS} statedb.cpp)
target_link_libraries(koinos_statedb koinos_exception koinos_types koinos_crypto mira)
target_include_directories(koinos_statedb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
