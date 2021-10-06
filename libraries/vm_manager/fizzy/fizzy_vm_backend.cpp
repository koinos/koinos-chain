
#include <fizzy/fizzy.h>

#include <koinos/exception.hpp>

#include <koinos/vm_manager/fizzy/exceptions.hpp>
#include <koinos/vm_manager/fizzy/fizzy_vm_backend.hpp>

#include <exception>
#include <optional>
#include <string>
#include <iostream>

#define FIZZY_MAX_CALL_DEPTH   251

namespace koinos::vm_manager::fizzy {

/**
 * Convert a pointer from inside the VM to a native pointer.
 */
char* resolve_ptr( FizzyInstance* fizzy_instance, uint32_t ptr, uint32_t size )
{
   KOINOS_ASSERT( fizzy_instance != nullptr, null_argument_exception, "fizzy_instance was unexpectedly null pointer" );
   size_t mem_size = fizzy_get_instance_memory_size( fizzy_instance );
   char* mem_data = (char *) fizzy_get_instance_memory_data( fizzy_instance );
   KOINOS_ASSERT( mem_data != nullptr, fizzy_returned_null_exception, "fizzy_get_instance_memory_data() unexpectedly returned null pointer" );

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

std::string fizzy_error_code_name(FizzyErrorCode code) noexcept
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
         return "UnknownFizzyError";
   }
}

class fizzy_runner
{
   public:
      fizzy_runner( abstract_host_api& h ) : _hapi(h) {}
      ~fizzy_runner();

      void parse_bytecode( char* bytecode_data, size_t bytecode_size );
      void instantiate_module();
      void call_start();

      FizzyExecutionResult _invoke_thunk( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
      FizzyExecutionResult _invoke_system_call( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;

   private:
      abstract_host_api&     _hapi;
      const FizzyModule*     _module = nullptr;
      FizzyInstance*         _instance = nullptr;
      FizzyExecutionContext* _fizzy_context = nullptr;
      int64_t                _previous_ticks;
      std::exception_ptr     _exception;
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
         // While testing the changes for #400, I was getting malloc errors that resolved themselves
         // by commenting out this line (freeing already freed memory). It would appear that fizzy_initialize
         // takes ownership even on failure and this line is not needed.
         // The bug I ran in to could be replicated via an uploaded contract and is therefore and attack vector.
         // fizzy_free_module( _module );
      }
   }
   if( _fizzy_context != nullptr )
      fizzy_free_execution_context(_fizzy_context);
}

void fizzy_runner::parse_bytecode( char* bytecode_data, size_t bytecode_size )
{
   KOINOS_ASSERT( bytecode_data != nullptr, fizzy_returned_null_exception, "fizzy_instance was unexpectedly null pointer" );
   KOINOS_ASSERT( _module == nullptr, runner_state_exception, "_module was unexpectedly non-null" );
   _module = fizzy_parse((uint8_t *) bytecode_data, bytecode_size, nullptr);
   KOINOS_ASSERT( _module != nullptr, module_parse_exception, "could not parse fizzy module" );
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
   FizzyExternalFunction invoke_thunk_fn = {{ FizzyValueTypeI32, invoke_thunk_arg_types, invoke_thunk_num_args }, invoke_thunk, this };

   FizzyExternalFn invoke_system_call = [](void* voidptr_context, FizzyInstance* fizzy_instance, const FizzyValue* args, FizzyExecutionContext* fizzy_context) noexcept -> FizzyExecutionResult
   {
      fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
      return runner->_invoke_system_call( args, fizzy_context );
   };

   FizzyValueType invoke_system_call_arg_types[] = {FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32, FizzyValueTypeI32};
   size_t invoke_system_call_num_args = 5;
   FizzyExternalFunction invoke_system_call_fn = {{ FizzyValueTypeI32, invoke_system_call_arg_types, invoke_system_call_num_args }, invoke_system_call, this };

   size_t num_host_funcs = 2;
   FizzyImportedFunction host_funcs[] = {{"env", "invoke_thunk", invoke_thunk_fn}, {"env", "invoke_system_call", invoke_system_call_fn}};

   FizzyError fizzy_err;

   size_t memory_pages_limit = 512;     // Number of 64k pages allowed to allocate

   KOINOS_ASSERT( _instance == nullptr, runner_state_exception, "_instance was unexpectedly non-null" );
   _instance = fizzy_resolve_instantiate(_module, host_funcs, num_host_funcs, nullptr, nullptr, nullptr, 0, memory_pages_limit, &fizzy_err);
   if( _instance == nullptr )
   {
      std::string error_code = fizzy_error_code_name( fizzy_err.code );
      std::string error_message = fizzy_err.message;
      KOINOS_THROW( module_instantiate_exception, "could not instantiate module - ${code}: ${msg}", ("code", error_code)("msg", error_message) );
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

      KOINOS_ASSERT( ret_ptr != nullptr, wasm_memory_exception, "invalid ret_ptr in invoke_thunk()" );
      KOINOS_ASSERT( arg_ptr != nullptr, wasm_memory_exception, "invalid arg_ptr in invoke_thunk()" );

      int64_t* ticks = fizzy_get_execution_context_ticks(_fizzy_context);
      KOINOS_ASSERT( ticks != nullptr, fizzy_returned_null_exception, "fizzy_get_execution_context_ticks() unexpectedly returned null pointer" );
      _hapi.use_meter_ticks( uint64_t( _previous_ticks - *ticks ) );

      try
      {
         result.value.i32 = _hapi.invoke_thunk( tid, ret_ptr, ret_len, arg_ptr, arg_len );
         result.has_value = true;
      }
      catch ( ... )
      {
         _exception = std::current_exception();
      }

      _previous_ticks = _hapi.get_meter_ticks();
      *ticks = _previous_ticks;
   }
   catch ( ... )
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

      KOINOS_ASSERT( ret_ptr != nullptr, wasm_memory_exception, "invalid ret_ptr in invoke_system_call()" );
      KOINOS_ASSERT( arg_ptr != nullptr, wasm_memory_exception, "invalid arg_ptr in invoke_system_call()" );
      int64_t* ticks = fizzy_get_execution_context_ticks(_fizzy_context);
      KOINOS_ASSERT( ticks != nullptr, fizzy_returned_null_exception, "fizzy_get_execution_context_ticks() unexpectedly returned null pointer" );
      _hapi.use_meter_ticks( uint64_t( _previous_ticks - *ticks ) );

      try
      {
         result.value.i32 = _hapi.invoke_system_call( xid, ret_ptr, ret_len, arg_ptr, arg_len );
         result.has_value = true;
      }
      catch ( ... )
      {
         _exception = std::current_exception();
      }

      _previous_ticks = _hapi.get_meter_ticks();
      *ticks = _previous_ticks;
   }
   catch ( ... )
   {
      _exception = std::current_exception();
   }

   result.trapped = !!_exception;
   return result;
}

void fizzy_runner::call_start()
{
   KOINOS_ASSERT( _fizzy_context == nullptr, runner_state_exception, "_fizzy_context was unexpectedly non-null" );
   _previous_ticks = _hapi.get_meter_ticks();
   _fizzy_context = fizzy_create_metered_execution_context( FIZZY_MAX_CALL_DEPTH, _previous_ticks );
   KOINOS_ASSERT( _fizzy_context != nullptr, create_context_exception, "could not create execution context" );

   uint32_t start_func_idx = 0;
   bool success = fizzy_find_exported_function_index( _module, "_start", &start_func_idx );
   KOINOS_ASSERT( success, module_start_exception, "module does not have _start function" );

   FizzyExecutionResult result = fizzy_execute( _instance, start_func_idx, nullptr, _fizzy_context );

   int64_t* ticks = fizzy_get_execution_context_ticks(_fizzy_context);
   KOINOS_ASSERT( ticks != nullptr, fizzy_returned_null_exception, "fizzy_get_execution_context_ticks() unexpectedly returned null pointer" );
   _hapi.use_meter_ticks( uint64_t( _previous_ticks - *ticks ) );

   if( _exception )
   {
      std::exception_ptr exc = _exception;
      _exception = std::exception_ptr();
      std::rethrow_exception( exc );
   }

   if( result.trapped )
   {
      KOINOS_THROW( wasm_trap_exception, "module exited due to trap" );
   }
}

void fizzy_vm_backend::run( abstract_host_api& hapi, char* bytecode_data, size_t bytecode_size )
{
   fizzy_runner runner(hapi);
   runner.parse_bytecode( bytecode_data, bytecode_size );
   runner.instantiate_module();
   runner.call_start();
}

} // koinos::vm_manager::fizzy
