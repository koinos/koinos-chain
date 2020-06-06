find_package(Boost REQUIRED COMPONENTS program_options )

add_executable(koinos-vm driver.cpp)

target_link_libraries(koinos-vm koinos_chain Boost::program_options)
