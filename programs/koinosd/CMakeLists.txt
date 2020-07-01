find_package(Boost REQUIRED COMPONENTS program_options )

add_executable( koinosd main.cpp )
target_link_libraries( koinosd koinos_exception appbase koinos_plugins koinos_crypto koinos_chain_plugin koinos_log Boost::program_options )
install( TARGETS
   koinosd
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
