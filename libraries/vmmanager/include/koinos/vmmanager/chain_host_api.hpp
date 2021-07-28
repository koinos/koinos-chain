#pragma once

#include <cstdint>

namespace koinos::vmmmanager {

class chain_host_api
{
   public:
      chain_host_api();
      virtual ~chain_host_api();

      virtual void invoke_thunk( uint32_t tid, uint8_t* ret_ptr, uint32_t ret_len, const uint8_t* arg_ptr, uint32_t arg_len ) = 0;
      virtual void invoke_system_call( uint32_t xid, uint8_t* ret_ptr, uint32_t ret_len, const uint8_t* arg_ptr, uint32_t arg_len ) = 0;
};

}
