#pragma once

#include <koinos/chain/constants.hpp>
#include <koinos/mq/client.hpp>
#include <koinos/rpc/chain/chain_rpc.pb.h>
#include <koinos/state_db/state_db_types.hpp>

#include <any>
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>

namespace koinos::chain {

namespace detail { class controller_impl; }

using genesis_data = std::map< std::pair< state_db::object_space, state_db::object_key >, state_db::object_value >;

class controller final
{
   public:
      controller();
      ~controller();

      void open( const std::filesystem::path& p, const std::any& o, const genesis_data& data, bool reset );
      void set_client( std::shared_ptr< mq::client > c );

      rpc::chain::submit_block_response submit_block(
         const rpc::chain::submit_block_request&,
         uint64_t index_to = 0,
         std::chrono::system_clock::time_point now = std::chrono::system_clock::now()
      );
      rpc::chain::submit_transaction_response submit_transaction( const rpc::chain::submit_transaction_request&  );
      rpc::chain::get_head_info_response      get_head_info(      const rpc::chain::get_head_info_request&  = {} );
      rpc::chain::get_chain_id_response       get_chain_id(       const rpc::chain::get_chain_id_request&   = {} );
      rpc::chain::get_fork_heads_response     get_fork_heads(     const rpc::chain::get_fork_heads_request& = {} );
      rpc::chain::read_contract_response      read_contract(      const rpc::chain::read_contract_request& );
      rpc::chain::get_account_nonce_response  get_account_nonce(  const rpc::chain::get_account_nonce_request& );

   private:
      std::unique_ptr< detail::controller_impl > _my;
};

} // koinos::chain
