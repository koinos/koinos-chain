add_executable(koinos_chain main.cpp)
target_link_libraries(koinos_chain Koinos::exception Koinos::crypto Koinos::proto Koinos::log Koinos::mq Koinos::util Koinos::chain Boost::program_options Koinos::git yaml-cpp)
install(TARGETS
   koinos_chain
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
