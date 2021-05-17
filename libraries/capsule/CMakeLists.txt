file(GLOB HEADERS "include/koinos/capsule/*.hpp" "include/koinos/capsule/wasm/*.hpp")
add_library(koinos_capsule_lib
            capsule.cpp
            ${HEADERS})
target_link_libraries(koinos_capsule_lib Koinos::exception Koinos::crypto Koinos::log)
target_include_directories(koinos_capsule_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(Koinos::capsule ALIAS koinos_capsule_lib)
