find_package(Boost COMPONENTS log filesystem REQUIRED)
file(GLOB HEADERS "include/koinos/statedb/*.hpp" "include/koinos/statedb/detail/*.hpp")
add_library(koinos_statedb ${HEADERS} statedb.cpp)
target_link_libraries(koinos_statedb Koinos::exception Koinos::types Koinos::crypto mira)
target_include_directories(koinos_statedb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(Koinos::statedb ALIAS koinos_statedb)

