
#include <koinos/vmmanager/eos/apply_context.hpp>
#include <koinos/vmmanager/eos/eos_host.hpp>
#include <koinos/vmmanager/eos/eos_vm_backend.hpp>
#include <koinos/vmmanager/eos/types.hpp>

namespace koinos::vmmanager::eos {

eos_vm_backend::eos_vm_backend() {}
eos_vm_backend::~eos_vm_backend() {}

std::string eos_vm_backend::backend_name()
{
   return "eos";
}

void eos_vm_backend::initialize()
{
   register_host_functions();
}

void eos_vm_backend::run( void* context, char* bytecode_data, size_t bytecode_size )
{
   // bytecode_data cannot be const due to limitation in eos guarded_ptr
   wasm_allocator_type wa;
   wasm_code_ptr bytecode_ptr( (unsigned char*) bytecode_data, bytecode_size );
   backend_type backend( bytecode_ptr, bytecode_ptr.bounds(), registrar_type{} );
   eos_apply_context eos_ctx;

   eos_ctx.user_context = context;

   backend.set_wasm_allocator( &wa );
   backend.initialize();

   try
   {
      backend( &eos_ctx, "env", "_start" );
   }
   catch( ... )
   {
      wa.free();
      throw;
   }

   wa.free();
}

}
