
#include <koinos/vmmanager/eos/common.hpp>
#include <koinos/vmmanager/eos/eos_context.hpp>

#include <cstdint>

namespace koinos::vmmanager::eos {

/**
 * Host functions provided by EOS must be class members.
 * The EOS VM requires that any pointer argument must be array_ptr<T>, and
 * must immediately be followed by a length.
 *
 * The purpose of eos_host_api is to provide such an adapter class.
 */
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
