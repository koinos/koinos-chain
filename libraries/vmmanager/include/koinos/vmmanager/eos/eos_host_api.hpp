
#include <koinos/vmmanager/eos/apply_context.hpp>
#include <koinos/vmmanager/eos/common.hpp>

#include <cstdint>

namespace koinos::vmmanager::eos {

struct eos_host_api final
{
   eos_host_api( eos_apply_context& ctx );
   eos_apply_context& context;

   void invoke_thunk(
      uint32_t tid,
      array_ptr< char > ret_ptr,
      uint32_t ret_len,
      array_ptr< const char > arg_ptr,
      uint32_t arg_len );

   void invoke_system_call(
      uint32_t sid,
      array_ptr< char > ret_ptr,
      uint32_t ret_len,
      array_ptr< const char > arg_ptr,
      uint32_t arg_len );
};

}
