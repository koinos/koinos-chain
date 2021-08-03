
#include <fizzy/fizzy.h>

#include <koinos/exception.hpp>

#include <koinos/vmmanager/fizzy/exceptions.hpp>
#include <koinos/vmmanager/fizzy/fizzy_vm_backend.hpp>

#include <exception>
#include <optional>
#include <string>

#define FIZZY_MAX_CALL_DEPTH   251

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

class fizzy_runner
{
   public:
      fizzy_runner( context& c ) : _ctx(c) {}
      ~fizzy_runner();

      void parse_bytecode( char* bytecode_data, size_t bytecode_size );
      void instantiate_module();
      void call_start();

      FizzyExecutionResult _invoke_thunk( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
      FizzyExecutionResult _invoke_system_call( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;

   private:
      context& _ctx;
      const FizzyModule* _module = nullptr;
      FizzyInstance* _instance = nullptr;
      FizzyExecutionContext* _fizzy_context = nullptr;
      std::exception_ptr _exception;
};

fizzy_runner::~fizzy_runner()
{
   if( _instance != nullptr )
   {
      fizzy_free_instance( _instance );
      _instance = nullptr;
      // As per fizzy docs, don't free _module if _instance was created successfully
      _module = nullptr;
   }
   else
   {
      if( _module != nullptr )
      {
         fizzy_free_module( _module );
      }
   }
   if( _fizzy_context != nullptr )
      fizzy_free_execution_context(_fizzy_context);
}

void fizzy_runner::parse_bytecode( char* bytecode_data, size_t bytecode_size )
{
   // TODO:  Add a caching system to recycle parsed modules
   _module = fizzy_parse((uint8_t *) bytecode_data, bytecode_size, nullptr);
   KOINOS_ASSERT( _module != nullptr, module_parse_exception, "Could not parse Fizzy module" );
}

void fizzy_runner::instantiate_module()
{
   FizzyExternalFn invoke_thunk = [](void* voidptr_context, FizzyInstance* fizzy_instance, const FizzyValue* args, FizzyExecutionContext* fizzy_context) noexcept -> FizzyExecutionResult
   {
      fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
      return runner->_invoke_thunk( args, fizzy_context );
   };

   FizzyValueType invoke_thunk_arg_types[] = {FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32};
   size_t invoke_thunk_num_args = 5;
   FizzyExternalFunction invoke_thunk_fn = {{ FizzyValueTypeVoid, invoke_thunk_arg_types, invoke_thunk_num_args }, invoke_thunk, this };

   FizzyExternalFn invoke_system_call = [](void* voidptr_context, FizzyInstance* fizzy_instance, const FizzyValue* args, FizzyExecutionContext* fizzy_context) noexcept -> FizzyExecutionResult
   {
      fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
      return runner->_invoke_system_call( args, fizzy_context );
   };

   FizzyValueType invoke_system_call_arg_types[] = {FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32};
   size_t invoke_system_call_num_args = 5;
   FizzyExternalFunction invoke_system_call_fn = {{ FizzyValueTypeVoid, invoke_system_call_arg_types, invoke_system_call_num_args }, invoke_system_call, this };

   size_t num_host_funcs = 2;
   FizzyImportedFunction host_funcs[] = {{"env", "invoke_thunk", invoke_thunk_fn}, {"env", "invoke_system_call", invoke_system_call_fn}};

   FizzyError fizzy_err;

   // TODO:  Make memory_pages_limit configurable
   size_t memory_pages_limit = 512;     // Number of 64k pages allowed to allocate

   _instance = fizzy_resolve_instantiate(_module, host_funcs, num_host_funcs, nullptr, nullptr, nullptr, 0, memory_pages_limit, &fizzy_err);
   if( _instance == nullptr )
   {
      std::string error_code = fizzy_error_code_name( fizzy_err.code );
      std::string error_message = fizzy_err.message;
      KOINOS_THROW( module_instantiate_exception, "Could not instantiate module - ${code}: ${msg}", ("code", error_code)("msg", error_message) );
   }
}

FizzyExecutionResult fizzy_runner::_invoke_thunk( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept
{
   FizzyExecutionResult result;
   result.has_value = false;
   result.value.i64 = 0;

   _exception = std::exception_ptr();

   try
   {
      uint32_t tid = args[0].i32;
      uint32_t ret_len = args[2].i32;
      char* ret_ptr = resolve_ptr(_instance, args[1].i32, ret_len);
      uint32_t arg_len = args[4].i32;
      const char* arg_ptr = resolve_ptr(_instance, args[3].i32, arg_len);

      KOINOS_ASSERT( ret_ptr != nullptr, wasm_memory_exception, "Invalid ret_ptr in invoke_thunk()" );
      KOINOS_ASSERT( arg_ptr != nullptr, wasm_memory_exception, "Invalid arg_ptr in invoke_thunk()" );

      _ctx._api_handler->invoke_thunk( tid, ret_ptr, ret_len, arg_ptr, arg_len );
   }
   catch(...)
   {
      _exception = std::current_exception();
   }

   result.trapped = !!_exception;
   return result;
}

FizzyExecutionResult fizzy_runner::_invoke_system_call( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept
{
   FizzyExecutionResult result;
   result.has_value = false;
   result.value.i64 = 0;

   _exception = std::exception_ptr();

   try
   {
      uint32_t xid = args[0].i32;
      uint32_t ret_len = args[2].i32;
      char* ret_ptr = resolve_ptr(_instance, args[1].i32, ret_len);
      uint32_t arg_len = args[4].i32;
      const char* arg_ptr = resolve_ptr(_instance, args[3].i32, arg_len);

      KOINOS_ASSERT( ret_ptr != nullptr, wasm_memory_exception, "Invalid ret_ptr in invoke_thunk()" );
      KOINOS_ASSERT( arg_ptr != nullptr, wasm_memory_exception, "Invalid arg_ptr in invoke_thunk()" );

      _ctx._api_handler->invoke_system_call( xid, ret_ptr, ret_len, arg_ptr, arg_len );
   }
   catch(...)
   {
      _exception = std::current_exception();
   }

   result.trapped = !!_exception;
   return result;
}

void fizzy_runner::call_start()
{
   _fizzy_context = fizzy_create_metered_execution_context( FIZZY_MAX_CALL_DEPTH, _ctx._meter_ticks );
   KOINOS_ASSERT( _fizzy_context != nullptr, create_context_exception, "Could not create execution context" );

   uint32_t start_func_idx = 0;
   bool success = fizzy_find_exported_function_index( _module, "_start", &start_func_idx );
   KOINOS_ASSERT( success, module_start_exception, "Module does not have _start function" );

   FizzyExecutionResult result = fizzy_execute( _instance, start_func_idx, nullptr, _fizzy_context );
   if( _exception )
   {
      std::exception_ptr exc = _exception;
      _exception = std::exception_ptr();
      std::rethrow_exception( exc );
   }

   _ctx._meter_ticks = *fizzy_get_execution_context_ticks(_fizzy_context);

   if( result.trapped )
   {
      if( _ctx._meter_ticks < 0 )
      {
         KOINOS_THROW( tick_meter_exception, "Ran out of ticks" );
      }
      KOINOS_THROW( wasm_trap_exception, "Module exited due to trap" );
   }
}

void fizzy_vm_backend::run( context& ctx, char* bytecode_data, size_t bytecode_size )
{
   fizzy_runner runner(ctx);
   runner.parse_bytecode( bytecode_data, bytecode_size );
   runner.instantiate_module();
   runner.call_start();
}

}
