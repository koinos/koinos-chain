file(GLOB HEADERS "include/koinos/fork/*.hpp")
add_library(fork INTERFACE)
target_sources(fork INTERFACE ${HEADERS})

target_link_libraries(fork INTERFACE koinos_exception)
target_include_directories(fork INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include")
