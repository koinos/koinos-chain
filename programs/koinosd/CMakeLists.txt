add_executable( koinosd main.cpp )
target_link_libraries( koinosd koinos_exception koinos_appbase koinos_plugins koinos_crypto koinos_chain_plugin fc koinos_log )
install( TARGETS
   koinosd
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
