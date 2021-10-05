file(GLOB HEADERS "include/koinos/chain/*.hpp" "include/koinos/chain/wasm/*.hpp")
add_library(koinos_chain_lib
            apply_context.cpp
            controller.cpp
            host_api.cpp
            system_calls.cpp
            thunk_dispatcher.cpp
            resources.cpp
            ${HEADERS})
target_link_libraries(koinos_chain_lib Koinos::state_db Koinos::exception Koinos::crypto Koinos::log Koinos::util Koinos::mq Koinos::vm_manager mira)
target_include_directories(koinos_chain_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(Koinos::chain ALIAS koinos_chain_lib)
