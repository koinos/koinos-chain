file(GLOB HEADERS "include/koinos/plugins/block_producer/*.hpp" "include/koinos/plugins/block_producer/util/*.hpp")
add_library(koinos_block_producer_plugin
            block_producer_plugin.cpp
            util/block_util.cpp
	    ${HEADERS})

target_link_libraries(koinos_block_producer_plugin appbase Koinos::chain Koinos::types Koinos::chain_plugin Koinos::crypto Koinos::log Koinos::util)
target_include_directories(koinos_block_producer_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(Koinos::block_producer_plugin ALIAS koinos_block_producer_plugin)
install(TARGETS
   koinos_block_producer_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

install(FILES ${HEADERS} DESTINATION "include/koinos/plugins/block_producer")

