add_executable( koinosd main.cpp )
target_link_libraries( koinosd koinos_exception appbase log )
install( TARGETS
   koinosd
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
