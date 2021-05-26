#pragma once

#include <koinos/mq/client.hpp>
#include <koinos/statedb/statedb_types.hpp>
#include <koinos/pack/classes.hpp>

#include <any>
#include <filesystem>
#include <map>
#include <memory>

namespace koinos::chain {

namespace detail { class controller_impl; }

using genesis_data = std::map< std::pair< statedb::object_space, statedb::object_key >, statedb::object_value >;

#define KOINOS_STATEDB_SPACE        0
#define KOINOS_STATEDB_CHAIN_ID_KEY 0

class controller final
{
   public:
      controller();
      ~controller();

      void open( const std::filesystem::path& p, const std::any& o, const genesis_data& data, bool reset );
      void set_client( std::shared_ptr< mq::client > c );

      rpc::chain::submit_block_response       submit_block(       const rpc::chain::submit_block_request&, bool indexing = false );
      rpc::chain::submit_transaction_response submit_transaction( const rpc::chain::submit_transaction_request&  );
      rpc::chain::get_head_info_response      get_head_info(      const rpc::chain::get_head_info_request&  = {} );
      rpc::chain::get_chain_id_response       get_chain_id(       const rpc::chain::get_chain_id_request&   = {} );
      rpc::chain::get_fork_heads_response     get_fork_heads(     const rpc::chain::get_fork_heads_request& = {} );
      rpc::chain::read_contract_response      read_contract(      const rpc::chain::read_contract_request& );

   private:
      std::unique_ptr< detail::controller_impl > _my;
};

} // koinos::chain
