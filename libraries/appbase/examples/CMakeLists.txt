add_executable( koinos_appbase_example main.cpp )
target_link_libraries( koinos_appbase_example koinos_appbase ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
