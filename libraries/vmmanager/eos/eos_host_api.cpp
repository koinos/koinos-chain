
#include <koinos/vmmanager/eos/eos_host_api.hpp>

namespace koinos::vmmanager::eos {

eos_host_api::eos_host_api( eos_context& ctx ) : _eos_context(ctx) {}

void eos_host_api::invoke_thunk(
      uint32_t tid,
      array_ptr< char > ret_ptr,
      uint32_t ret_len,
      array_ptr< const char > arg_ptr,
      uint32_t arg_len )
{
   _eos_context._context->_api_handler->invoke_thunk( tid, ret_ptr.value, ret_len, arg_ptr.value, arg_len );
}

void eos_host_api::invoke_system_call(
      uint32_t sid,
      array_ptr< char > ret_ptr,
      uint32_t ret_len,
      array_ptr< const char > arg_ptr,
      uint32_t arg_len )
{
   _eos_context._context->_api_handler->invoke_system_call( sid, ret_ptr.value, ret_len, arg_ptr.value, arg_len );
}

}
