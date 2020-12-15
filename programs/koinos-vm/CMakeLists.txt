find_package(Boost CONFIG REQUIRED COMPONENTS program_options)

add_executable(koinos-vm driver.cpp)
target_link_libraries(koinos-vm PUBLIC koinos_chain Boost::program_options)

