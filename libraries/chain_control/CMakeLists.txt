file(GLOB HEADERS "include/koinos/chain_control/*.hpp")
add_library(chain_control ${HEADERS} chain_control.cpp)
target_include_directories(chain_control PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(chain_control statedb fork pack koinos_exception koinos_crypto chain mira )
