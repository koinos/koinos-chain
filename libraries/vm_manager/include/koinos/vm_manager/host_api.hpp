#pragma once

#include <cstdint>

namespace koinos::vm_manager {

/**
 * An abstract class representing an implementation of an API.
 *
 * The user of the vm_manager library is responsible for creating an application-specific subclass.
 */
class abstract_host_api
{
   public:
      abstract_host_api();
      virtual ~abstract_host_api();

      virtual int32_t invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) = 0;
      virtual int32_t invoke_system_call( uint32_t xid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) = 0;
      virtual int64_t get_meter_ticks()const = 0;
      virtual void use_meter_ticks( uint64_t meter_ticks ) = 0;
};

} // koinos::vm_manager
