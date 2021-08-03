#pragma once

#include <cstdint>

namespace koinos::vmmanager {

/**
 * An abstract class representing an implementation of an API.
 *
 * The user of the vmmanager library is responsible for creating an application-specific subclass.
 */
class api_handler
{
   public:
      api_handler();
      virtual ~api_handler();

      virtual void invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) = 0;
      virtual void invoke_system_call( uint32_t xid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) = 0;
};

} // koinos::vmmanager
