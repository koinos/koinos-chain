file(GLOB HEADERS "include/koinos/plugins/block_producer/*.hpp")
add_library( block_producer_plugin
             block_producer_plugin.cpp
	     ${HEADERS} )

target_link_libraries( block_producer_plugin chain_control chain chainbase appbase pack chain_plugin koinos_crypto )
target_include_directories( block_producer_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   block_producer_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

install( FILES ${HEADERS} DESTINATION "include/koinos/plugins/block_producer" )
