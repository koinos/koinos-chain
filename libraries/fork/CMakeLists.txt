file(GLOB HEADERS "include/koinos/fork/*.hpp")
add_library(koinos_fork INTERFACE)
target_sources(koinos_fork INTERFACE ${HEADERS})

target_link_libraries(koinos_fork INTERFACE koinos_exception)
target_include_directories(koinos_fork INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include")
