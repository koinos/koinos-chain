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

      apply_context& _ctx;

      virtual uint32_t invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) override;
      virtual uint32_t invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len ) override;
      virtual int64_t get_meter_ticks() const override;
      virtual void use_meter_ticks( uint64_t meter_ticks ) override;
};

} // koinos::chain
