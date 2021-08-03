
#include <koinos/vmmanager/eos/common.hpp>
#include <koinos/vmmanager/eos/eos_context.hpp>

#include <cstdint>

namespace koinos::vmmanager::eos {

struct eos_host_api final
{
   eos_host_api( eos_context& ctx );
   eos_context& _eos_context;

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

} // koinos::vmmanager::eos
