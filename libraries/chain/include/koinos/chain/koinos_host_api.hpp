#pragma once

#include <koinos/vmmanager/chain_host_api.hpp>

namespace koinos::chain {

class koinos_host_api : public vmmanager::chain_host_api
{
   public:
      koinos_host_api();
      virtual ~koinos_host_api() override;

      virtual void invoke_thunk( uint32_t tid, uint8_t* ret_ptr, uint32_t ret_len, const uint8_t* arg_ptr, uint32_t arg_len ) override;
      virtual void invoke_system_call( uint32_t xid, uint8_t* ret_ptr, uint32_t ret_len, const uint8_t* arg_ptr, uint32_t arg_len ) override;
};

}
