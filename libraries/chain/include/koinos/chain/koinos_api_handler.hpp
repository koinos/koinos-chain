#pragma once

#include <koinos/vmmanager/api_handler.hpp>

namespace koinos::chain {

class apply_context;

class koinos_api_handler : public vmmanager::api_handler
{
   public:
      koinos_api_handler( apply_context& ctx );
      virtual ~koinos_api_handler() override;

      virtual void invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) override;
      virtual void invoke_system_call( uint32_t xid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) override;

      apply_context& context;
};

} // koinos::chain
