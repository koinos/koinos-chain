file(GLOB HEADERS "include/koinos/statedb/*.hpp")
add_library(statedb ${HEADERS} statedb.cpp)
target_link_libraries( statedb strpolate koinos_exception chainbase pack fc mira )
target_include_directories(statedb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
