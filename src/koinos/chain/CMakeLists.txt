file(GLOB HEADERS "include/koinos/chain/*.hpp" "include/koinos/chain/wasm/*.hpp")
add_library(koinos_chain_lib
            controller.cpp
            chronicler.cpp
            execution_context.cpp
            host_api.cpp
            indexer.cpp
            proto_utils.cpp
            session.cpp
            system_calls.cpp
            thunk_dispatcher.cpp
            resource_meter.cpp
            state.cpp
            ${HEADERS})
target_link_libraries(koinos_chain_lib Koinos::state_db Koinos::exception Koinos::crypto Koinos::log Koinos::util Koinos::mq Koinos::vm_manager)
target_include_directories(koinos_chain_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(Koinos::chain ALIAS koinos_chain_lib)
