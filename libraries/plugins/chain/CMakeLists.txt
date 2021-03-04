file(GLOB HEADERS "include/koinos/plugins/chain/*.hpp")
add_library(koinos_chain_plugin
            chain_plugin.cpp
            reqhandler.cpp
            ${HEADERS})

target_link_libraries(koinos_chain_plugin appbase Koinos::chain Koinos::mq Koinos::net Koinos::crypto Koinos::log Koinos::util)
target_include_directories(koinos_chain_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
add_library(Koinos::chain_plugin ALIAS koinos_chain_plugin)
install(TARGETS
   koinos_chain_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
install(FILES ${HEADERS} DESTINATION "include/koinos/plugins/chain")
