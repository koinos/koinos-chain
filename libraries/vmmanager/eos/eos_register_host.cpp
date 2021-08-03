
#include <koinos/vmmanager/eos/eos_host.hpp>
#include <koinos/vmmanager/eos/eos_host_api.hpp>
#include <koinos/vmmanager/eos/types.hpp>

namespace koinos::vmmanager::eos {

void register_host_functions()
{
   registrar_type::add< eos_host_api, &eos_host_api::invoke_thunk, wasm_allocator_type >( "env", "invoke_thunk" );
   registrar_type::add< eos_host_api, &eos_host_api::invoke_system_call, wasm_allocator_type >( "env", "invoke_system_call" );
}

} // koinos::vmmanager::eos
