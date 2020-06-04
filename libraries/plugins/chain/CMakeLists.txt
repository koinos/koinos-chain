
file(GLOB HEADERS "include/koinos/plugins/chain/*.hpp")
add_library( koinos_chain_plugin
             chain_plugin.cpp
             ${HEADERS} )

target_link_libraries( koinos_chain_plugin koinos_chain_control chainbase appbase koinos_log )
target_include_directories( koinos_chain_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   koinos_chain_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
install( FILES ${HEADERS} DESTINATION "include/koinos/plugins/chain" )
