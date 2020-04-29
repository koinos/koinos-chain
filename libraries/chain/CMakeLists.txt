file(GLOB HEADERS "include/koinos/chain/*.hpp" "include/koinos/chain/wasm/*.hpp")
add_library( chain
             apply_context.cpp
             name.cpp
             ${HEADERS} )
target_link_libraries( chain chainbase fc mira softfloat koinos_exception builtins )
target_include_directories( chain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
