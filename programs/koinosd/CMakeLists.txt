add_executable( koinosd main.cpp )
target_link_libraries( koinosd koinos_exception appbase koinos_plugins koinos_crypto koinos_chain_plugin fc koinos_log koinos_strpolate )
install( TARGETS
   koinosd
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
