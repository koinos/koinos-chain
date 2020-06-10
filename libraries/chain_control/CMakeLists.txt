file(GLOB HEADERS "include/koinos/chain_control/*.hpp")
add_library(koinos_chain_control ${HEADERS} chain_control.cpp)
target_include_directories(koinos_chain_control PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(koinos_chain_control koinos_statedb koinos_pack koinos_exception koinos_crypto koinos_chain mira koinos_log koinos_util )

