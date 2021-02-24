file(GLOB HEADERS "include/koinos/chain/*.hpp" "include/koinos/chain/wasm/*.hpp")
add_library(koinos_chain
            apply_context.cpp
            host.cpp
            mempool.cpp
            system_calls.cpp
            thunk_dispatcher.cpp
            thunks.cpp
            ${HEADERS})
target_link_libraries(koinos_chain Koinos::statedb Koinos::exception Koinos::crypto Koinos::log Koinos::util builtins eos-vm mira softfloat)
target_include_directories(koinos_chain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(Koinos::chain ALIAS koinos_chain)

