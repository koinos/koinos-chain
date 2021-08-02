
#include <koinos/vmmanager/eos/eos_host_api.hpp>

namespace koinos::vmmanager::eos {

eos_host_api::eos_host_api( eos_apply_context& ctx ) : context(ctx) {}

// TODO
void eos_host_api::invoke_thunk(
      uint32_t tid,
      array_ptr< char > ret_ptr,
      uint32_t ret_len,
      array_ptr< const char > arg_ptr,
      uint32_t arg_len ) {}

// TODO
void eos_host_api::invoke_system_call(
      uint32_t sid,
      array_ptr< char > ret_ptr,
      uint32_t ret_len,
      array_ptr< const char > arg_ptr,
      uint32_t arg_len ) {}

}
