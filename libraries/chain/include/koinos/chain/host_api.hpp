#pragma once

#include <type_traits>

#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/types.hpp>

namespace koinos::chain {

class host_api final : public vm_manager::abstract_host_api
{
   public:
      host_api( apply_context& ctx );
      virtual ~host_api() override;

   private:
      apply_context& _ctx;

   public:
      virtual void invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) override;
      virtual void invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) override;
};

} // koinos::chain
