file(GLOB HEADERS "include/koinos/statedb/*.hpp")
add_library(koinos_statedb ${HEADERS} statedb.cpp)
target_link_libraries(koinos_statedb koinos_strpolate koinos_exception chainbase fc mira)
target_include_directories(koinos_statedb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
