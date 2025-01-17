#pragma once

#include <koinos/chain/constants.hpp>
#include <koinos/mq/client.hpp>
#include <koinos/protocol/protocol.pb.h>
#include <koinos/rpc/chain/chain_rpc.pb.h>
#include <koinos/state_db/state_db_types.hpp>

#include <any>
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>

namespace koinos::chain {

namespace detail {
class controller_impl;
} // namespace detail

enum class fork_resolution_algorithm
{
  fifo,
  block_time,
  pob
};

class controller final
{
public:
  controller( uint64_t read_compute_bandwith_limit                = 0,
              uint32_t syscall_bufsize                            = 0,
              std::optional< uint64_t > pending_transaction_limit = {} );
  ~controller();

  void
  open( const std::filesystem::path& p, const chain::genesis_data& data, fork_resolution_algorithm algo, bool reset );
  void close();
  void set_client( std::shared_ptr< mq::client > c );

  rpc::chain::submit_block_response
  submit_block( const rpc::chain::submit_block_request&,
                uint64_t index_to                         = 0,
                std::chrono::system_clock::time_point now = std::chrono::system_clock::now() );
  rpc::chain::propose_block_response
  propose_block( const rpc::chain::propose_block_request&,
                 uint64_t index_to                         = 0,
                 std::chrono::system_clock::time_point now = std::chrono::system_clock::now() );
  rpc::chain::submit_transaction_response submit_transaction( const rpc::chain::submit_transaction_request& );
  rpc::chain::get_head_info_response get_head_info( const rpc::chain::get_head_info_request& = {} );
  rpc::chain::get_chain_id_response get_chain_id( const rpc::chain::get_chain_id_request& = {} );
  rpc::chain::get_fork_heads_response get_fork_heads( const rpc::chain::get_fork_heads_request& = {} );
  rpc::chain::read_contract_response read_contract( const rpc::chain::read_contract_request& );
  rpc::chain::get_account_nonce_response get_account_nonce( const rpc::chain::get_account_nonce_request& );
  rpc::chain::get_account_rc_response get_account_rc( const rpc::chain::get_account_rc_request& );
  rpc::chain::get_resource_limits_response get_resource_limits( const rpc::chain::get_resource_limits_request& );
  rpc::chain::invoke_system_call_response invoke_system_call( const rpc::chain::invoke_system_call_request& );

private:
  std::unique_ptr< detail::controller_impl > _my;
};

} // namespace koinos::chain
