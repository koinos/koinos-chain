#pragma once

#include <type_traits>

#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/types.hpp>

namespace koinos::chain {

class host_api final : public vm_manager::abstract_host_api
{
   public:
      host_api( execution_context& ctx );
      virtual ~host_api() override;

      execution_context& _ctx;

      virtual std::pair< int32_t, error_data > call( uint32_t sid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written ) override;
      virtual int32_t invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written ) override;
      virtual int32_t invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written  ) override;
      virtual int64_t get_meter_ticks() const override;
      virtual void use_meter_ticks( uint64_t meter_ticks ) override;
};

} // koinos::chain
