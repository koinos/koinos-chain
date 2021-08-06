#pragma once

#include <koinos/vm_manager/host_api.hpp>

namespace koinos::chain {

class apply_context;

/**
 * Implementation of vm_manager::host_api interface.
 *
 * Confusingly, the subclass and superclass are both called host_api.
 */
class host_api : public vm_manager::host_api
{
   public:
      host_api( apply_context& ctx );
      virtual ~host_api() override;

      virtual void invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) override;
      virtual void invoke_system_call( uint32_t xid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) override;

      apply_context& context;
};

} // koinos::chain
