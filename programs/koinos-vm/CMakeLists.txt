add_executable(koinos-vm driver.cpp)

target_link_libraries(koinos-vm eos-vm koinos_chain)
