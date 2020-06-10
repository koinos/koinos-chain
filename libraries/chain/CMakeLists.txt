file(GLOB HEADERS "include/koinos/chain/*.hpp" "include/koinos/chain/wasm/*.hpp")
add_library( koinos_chain
             apply_context.cpp
             system_calls.cpp
             ${HEADERS} )
target_link_libraries( koinos_chain eos-vm koinos_statedb mira softfloat koinos_exception koinos_crypto builtins koinos_log koinos_util )
target_include_directories( koinos_chain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
