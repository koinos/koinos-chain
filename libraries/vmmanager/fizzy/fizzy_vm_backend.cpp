
#include <fizzy/fizzy.h>

#include <koinos/exception.hpp>

#include <koinos/vmmanager/fizzy/exceptions.hpp>
#include <koinos/vmmanager/fizzy/fizzy_vm_backend.hpp>

#include <optional>
#include <string>

namespace koinos::vmmanager::fizzy {

/**
 * Convert a pointer from inside the VM to a native pointer.
 */
char* resolve_ptr( FizzyInstance* fizzy_instance, uint32_t ptr, uint32_t size )
{
   size_t mem_size = fizzy_get_instance_memory_size( fizzy_instance );
   char* mem_data = (char *) fizzy_get_instance_memory_data( fizzy_instance );

   if( ptr == mem_size )
   {
      if( size == 0 )
         return mem_data + mem_size;
   }
   else if( ptr > mem_size )
   {
      return nullptr;
   }

   // How much memory exists between pointer and end of memory?
   uint32_t mem_at_ptr = mem_size - ptr;
   if( uint64_t(mem_at_ptr) < uint64_t(size) )
      return nullptr;

   return mem_data + ptr;
}

fizzy_vm_backend::fizzy_vm_backend() {}
fizzy_vm_backend::~fizzy_vm_backend() {}

std::string fizzy_vm_backend::backend_name()
{
   return "fizzy";
}

void fizzy_vm_backend::initialize()
{
}

const char* fizzy_error_code_name(FizzyErrorCode code) noexcept
{
   switch( code )
   {
      case FizzySuccess:
         return "FizzySuccess";
      case FizzyErrorMalformedModule:
         return "FizzyErrorMalformedModule";
      case FizzyErrorInvalidModule:
         return "FizzyErrorInvalidModule";
      case FizzyErrorInstantiationFailed:
         return "FizzyErrorInstantiationFailed";
      case FizzyErrorMemoryAllocationFailed:
         return "FizzyErrorMemoryAllocationFailed";
      case FizzyErrorOther:
         return "FizzyErrorOther";
      default:
         return nullptr;
   }
}

void fizzy_vm_backend::run( context& ctx, char* bytecode_data, size_t bytecode_size )
{
   // TODO:  Add a caching system to recycle parsed modules
   const FizzyModule* module = fizzy_parse((uint8_t *) bytecode_data, bytecode_size, nullptr);
   KOINOS_ASSERT( module != nullptr, module_parse_exception, "Could not parse Fizzy module" );

   FizzyExternalFn invoke_thunk = [](void* voidptr_context, FizzyInstance* fizzy_instance, const FizzyValue* args, FizzyExecutionContext* fizzy_context) noexcept -> FizzyExecutionResult
   {
      FizzyExecutionResult result;
      result.trapped = true;
      result.has_value = false;
      result.value.i64 = 0;
      try
      {
         context* pctx = static_cast<context *>(voidptr_context);
         uint32_t tid = args[0].i32;
         uint32_t ret_len = args[2].i32;
         char* ret_ptr = resolve_ptr(fizzy_instance, args[1].i32, ret_len);
         uint32_t arg_len = args[4].i32;
         const char* arg_ptr = resolve_ptr(fizzy_instance, args[3].i32, arg_len);

         KOINOS_ASSERT( ret_ptr != nullptr, wasm_memory_exception, "Invalid ret_ptr in invoke_thunk()" );
         KOINOS_ASSERT( arg_ptr != nullptr, wasm_memory_exception, "Invalid arg_ptr in invoke_thunk()" );

         pctx->_chain_host_api->invoke_thunk( tid, ret_ptr, ret_len, arg_ptr, arg_len );
         result.trapped = false;
      }
      catch( const koinos::exception& e )
      {
         LOG(warning) << e.get_message() << std::endl;
      }
      catch ( const boost::exception& e )
      {
         LOG(warning) << boost::diagnostic_information( e ) << std::endl;
      }
      catch ( const std::exception& e )
      {
         LOG(warning) << e.what() << std::endl;
      }
      catch ( ... )
      {
         LOG(warning) << "Unknown exception" << std::endl;
      }
      return result;
   };

   FizzyExternalFn invoke_system_call = [](void* voidptr_context, FizzyInstance* fizzy_instance, const FizzyValue* args, FizzyExecutionContext* fizzy_context) noexcept -> FizzyExecutionResult
   {
      FizzyExecutionResult result;
      result.trapped = true;
      result.has_value = false;
      result.value.i64 = 0;
      try
      {
         context* pctx = static_cast<context *>(voidptr_context);
         uint32_t xid = args[0].i32;
         uint32_t ret_len = args[2].i32;
         char* ret_ptr = resolve_ptr(fizzy_instance, args[1].i32, ret_len);
         uint32_t arg_len = args[4].i32;
         const char* arg_ptr = resolve_ptr(fizzy_instance, args[3].i32, arg_len);

         KOINOS_ASSERT( ret_ptr != nullptr, wasm_memory_exception, "Invalid ret_ptr in invoke_system_call()" );
         KOINOS_ASSERT( arg_ptr != nullptr, wasm_memory_exception, "Invalid arg_ptr in invoke_system_call()" );

         pctx->_chain_host_api->invoke_system_call( xid, ret_ptr, ret_len, arg_ptr, arg_len );
         result.trapped = false;
      }
      catch( const koinos::exception& e )
      {
         LOG(warning) << e.get_message() << std::endl;
      }
      catch ( const boost::exception& e )
      {
         LOG(warning) << boost::diagnostic_information( e ) << std::endl;
      }
      catch ( const std::exception& e )
      {
         LOG(warning) << e.what() << std::endl;
      }
      catch ( ... )
      {
         LOG(warning) << "Unknown exception" << std::endl;
      }
      return result;
   };

   FizzyValueType invoke_thunk_arg_types[] = {FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32};
   size_t invoke_thunk_num_args = 5;
   FizzyExternalFunction invoke_thunk_fn = {{ FizzyValueTypeVoid, invoke_thunk_arg_types, invoke_thunk_num_args }, invoke_thunk, &ctx };

   FizzyValueType invoke_system_call_arg_types[] = {FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32};
   size_t invoke_system_call_num_args = 5;
   FizzyExternalFunction invoke_system_call_fn = {{ FizzyValueTypeVoid, invoke_system_call_arg_types, invoke_system_call_num_args }, invoke_system_call, &ctx };

   size_t num_host_funcs = 2;
   FizzyImportedFunction host_funcs[] = {{"env", "invoke_thunk", invoke_thunk_fn}, {"env", "invoke_system_call", invoke_system_call_fn}};

   FizzyError fizzy_err;

   // TODO:  Make memory_pages_limit configurable
   size_t memory_pages_limit = 512;     // Number of 64k pages allowed to allocate

   FizzyInstance* instance = fizzy_resolve_instantiate(module, host_funcs, num_host_funcs, nullptr, nullptr, nullptr, 0, memory_pages_limit, &fizzy_err);
   if( instance == nullptr )
   {
      std::string error_code = fizzy_error_code_name( fizzy_err.code );
      std::string error_message = fizzy_err.message;
      KOINOS_THROW( module_instantiate_exception, "Could not instantiate module - ${code}: ${msg}", ("code", error_code)("msg", error_message) );
   }

   uint32_t start_func_idx = 0;
   bool success = fizzy_find_exported_function_index( module, "_start", &start_func_idx );
   KOINOS_ASSERT( success, module_start_exception, "Module does not have _start function" );

   FizzyExecutionResult result = fizzy_execute( instance, start_func_idx, nullptr );
   // KOINOS_ASSERT( !result.trapped, wasm_trap_exception, "Module exited due to trap" );
}

}
