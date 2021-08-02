file(GLOB HEADERS "include/koinos/chain/*.hpp")
add_library(koinos_chain_lib
            apply_context.cpp
            controller.cpp
            host.cpp
            system_calls.cpp
            thunk_dispatcher.cpp
            ${HEADERS})
target_link_libraries(koinos_chain_lib Koinos::statedb Koinos::exception Koinos::crypto Koinos::log Koinos::util Koinos::mq Koinos::vmmanager mira)
target_include_directories(koinos_chain_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(Koinos::chain ALIAS koinos_chain_lib)
