#pragma once

#include <koinos/vmmanager/chain_host_api.hpp>

namespace koinos::chain {

class apply_context;

class koinos_host_api : public vmmanager::chain_host_api
{
   public:
      koinos_host_api( apply_context& ctx );
      virtual ~koinos_host_api() override;

      virtual void invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) override;
      virtual void invoke_system_call( uint32_t xid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) override;

      apply_context& context;
};

}
