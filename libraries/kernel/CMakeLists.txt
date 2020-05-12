file(GLOB HEADERS "include/koinos/kernel/*.hpp")
add_library( kernel
             syscalls.cpp
             ${HEADERS} )
target_link_libraries( kernel softfloat koinos_exception builtins )
target_include_directories( kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
