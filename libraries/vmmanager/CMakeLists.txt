file(GLOB HEADERS "include/koinos/vmmanager/*.hpp" "include/koinos/vmmanager/wasm/*.hpp")
add_library(koinos_vmmanager_lib
            chain_host_api.cpp
            vm_backend.cpp

            eos/eos_register_host.cpp
            eos/eos_host_api.cpp
            eos/eos_vm_backend.cpp

            ${HEADERS})
target_link_libraries(koinos_vmmanager_lib Koinos::exception Koinos::log Koinos::util eos-vm)
target_include_directories(koinos_vmmanager_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(Koinos::vmmanager ALIAS koinos_vmmanager_lib)
