#pragma once

#include <cstdint>

namespace koinos::vm_manager {

/**
 * An abstract class representing an implementation of an API.
 *
 * The user of the vm_manager library is responsible for creating an application-specific subclass.
 */
class host_api
{
   public:
      host_api();
      virtual ~host_api();

      virtual void invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) = 0;
      virtual void invoke_system_call( uint32_t xid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) = 0;
};

} // koinos::vm_manager
