
#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/constants.hpp>
#include <koinos/chain/controller.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/system_calls.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/json.hpp>

#include <koinos/state_db/state_db.hpp>

#include <koinos/util.hpp>

#include <koinos/vm_manager/vm_backend.hpp>

#include <mira/database_configuration.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <list>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <thread>

namespace koinos::chain {

using namespace std::string_literals;

using vectorstream = boost::interprocess::basic_vectorstream< std::vector< char > >;
using fork_data    = std::pair< std::vector< block_topology >, block_topology >;

namespace detail {

class controller_impl final
{
   public:
      controller_impl();
      ~controller_impl();

      void open( const std::filesystem::path& p, const std::any& o, const genesis_data& data, bool reset );
      void set_client( std::shared_ptr< mq::client > c );

      rpc::chain::submit_block_response submit_block(
         const rpc::chain::submit_block_request&,
         block_height_type index_to,
         std::chrono::system_clock::time_point now
      );
      rpc::chain::submit_transaction_response submit_transaction( const rpc::chain::submit_transaction_request& );
      rpc::chain::get_head_info_response      get_head_info(      const rpc::chain::get_head_info_request& );
      rpc::chain::get_chain_id_response       get_chain_id(       const rpc::chain::get_chain_id_request& );
      rpc::chain::get_fork_heads_response     get_fork_heads(     const rpc::chain::get_fork_heads_request& );
      rpc::chain::read_contract_response      read_contract(      const rpc::chain::read_contract_request & );
      rpc::chain::get_account_nonce_response  get_account_nonce(  const rpc::chain::get_account_nonce_request& );

   private:
      state_db::state_db             _state_db;
      std::shared_mutex             _state_db_mutex;
      std::shared_ptr< mq::client > _client;
      int64_t                       _max_read_cycles = KOINOS_MAX_METER_TICKS;
      std::shared_ptr< vm_manager::vm_backend > _vm_backend;

      fork_data get_fork_data();
      fork_data get_fork_data_lockless();
};

controller_impl::controller_impl()
{
   _vm_backend = vm_manager::get_vm_backend();
   KOINOS_ASSERT( _vm_backend.get() != nullptr, koinos::chain::unknown_backend_exception, "Couldn't get VM backend" );

   LOG(info) << "Initialized " << _vm_backend->backend_name() << " VM backend";

   _vm_backend->initialize();
}

controller_impl::~controller_impl()
{
   std::lock_guard< std::shared_mutex > lock( _state_db_mutex );
   _state_db.close();
}

void controller_impl::open( const std::filesystem::path& p, const std::any& o, const genesis_data& data, bool reset )
{
   std::lock_guard< std::shared_mutex > lock( _state_db_mutex );

   _state_db.open( p, o, [&]( state_db::state_node_ptr root )
   {
      for ( const auto& entry : data )
      {
         state_db::put_object_args put_args;
         put_args.space = entry.first.first;
         put_args.key = entry.first.second;
         put_args.buf = entry.second.data();
         put_args.object_size = entry.second.size();

         state_db::put_object_result put_res;
         root->put_object( put_res, put_args );

         KOINOS_ASSERT(
            !put_res.object_existed,
            koinos::chain::unexpected_state,
            "encountered unexpected object in initial state"
         );
      }
      LOG(info) << "Wrote " << data.size() << " genesis objects into new database";
   } );

   if ( reset )
   {
      LOG(info) << "Resetting database...";
      _state_db.reset();
   }

   auto head = _state_db.get_head();
   LOG(info) << "Opened database at block - Height: " << head->revision() << ", ID: " << head->id();
}

void controller_impl::set_client( std::shared_ptr< mq::client > c )
{
   _client = c;
}

rpc::chain::submit_block_response controller_impl::submit_block(
   const rpc::chain::submit_block_request& request,
   block_height_type index_to,
   std::chrono::system_clock::time_point now )
{
   std::lock_guard< std::shared_mutex > lock( _state_db_mutex );

   static constexpr uint64_t index_message_interval = 1000;
   static constexpr std::chrono::seconds time_delta = std::chrono::seconds( 5 );

   auto time_lower_bound = uint64_t( 0 );
   auto time_upper_bound = std::chrono::duration_cast< std::chrono::milliseconds >( ( now + time_delta ).time_since_epoch() ).count();
   block_height_type parent_height{ 0 };

   state_db::state_node_ptr block_node;

   block_node = _state_db.get_node( request.block.id );

   if ( block_node ) return {}; // Block has been applied

   if ( !index_to )
   {
      LOG(info) << "Pushing block - Height: " << request.block.header.height
         << ", ID: " << request.block.id;
   }

   block_node = _state_db.create_writable_node( request.block.header.previous, request.block.id );

   // If this is not the genesis case, we must ensure that the proposed block timestamp is greater
   // than the parent block timestamp.
   if ( block_node && !crypto::multihash_is_zero( request.block.header.previous ) )
   {
      apply_context parent_ctx( _vm_backend );

      // The following call to set_meter_ticks() does not set an upper bound on the cycles that
      // can be used in the block.  Rather, it provides some initial cycles in case the subsequent
      // system calls are overridden.  See issue #439.

      parent_ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);

      parent_ctx.push_frame( stack_frame {
         .call = crypto::hash( CRYPTO_RIPEMD160_ID, "submit_block"s ).digest,
         .call_privilege = privilege::kernel_mode
      } );

      auto parent_node = _state_db.get_node( request.block.header.previous );
      parent_ctx.set_state_node( parent_node );
      time_lower_bound = system_call::get_head_block_time( parent_ctx ).t;
      parent_height = system_call::get_head_info( parent_ctx ).head_topology.height;
   }

   apply_context ctx( _vm_backend );

   // The following call to set_meter_ticks() does not set an upper bound on the cycles that
   // can be used in the block.  Rather, it provides some initial cycles in case the subsequent
   // system calls are overridden.  See issue #439.
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);

   try
   {
      // Genesis case, when the first block is submitted the previous must be the zero hash
      if ( crypto::multihash_is_zero( request.block.header.previous ) )
      {
         KOINOS_ASSERT( request.block.header.height == 1, unexpected_height, "First block must have height of 1" );
      }

      KOINOS_ASSERT(
         crypto::hash_n( CRYPTO_SHA2_256_ID, request.block.header, request.block.active_data ) == request.block.id,
         block_id_mismatch,
         "Block contains an invalid block ID"
      );

      KOINOS_ASSERT( block_node, unknown_previous_block, "Unknown previous block" );

      KOINOS_ASSERT(
         request.block.header.height == parent_height + 1,
         unexpected_height,
         "Expected block height of ${a}, was ${b}", ("a", parent_height + 1)("b", request.block.header.height)
      );

      KOINOS_ASSERT( request.block.header.timestamp.t <= time_upper_bound, timestamp_out_of_bounds, "Block timestamp is too far in the future" );
      KOINOS_ASSERT( request.block.header.timestamp.t >= time_lower_bound, timestamp_out_of_bounds, "Block timestamp is too old" );

      ctx.push_frame( stack_frame {
         .call = crypto::hash( CRYPTO_RIPEMD160_ID, "submit_block"s ).digest,
         .call_privilege = privilege::kernel_mode
      } );

      ctx.set_state_node( block_node );

      system_call::apply_block(
         ctx,
         request.block,
         request.verify_passive_data,
         request.verify_block_signature,
         request.verify_transaction_signatures );

      if ( _client && _client->is_connected() )
      {
         rpc::block_store::block_store_request req = rpc::block_store::add_block_request {
            .block_to_add = block_store::block_item {
               .block_id     = request.block.id,
               .block_height = request.block.header.height,
               .block        = request.block
            }
         };

         pack::json j;
         pack::to_json( j, req );
         auto future = _client->rpc( service::block_store, j.dump(), 750 /* ms */, mq::retry_policy::none );

         rpc::block_store::block_store_response resp;
         pack::from_json( pack::json::parse( future.get() ), resp );

         std::visit( koinos::overloaded {
            [&]( const rpc::block_store::add_block_response& r )
            {
            },
            [&] ( const rpc::block_store::block_store_error_response& r )
            {
               KOINOS_THROW( koinos::exception, "Received error response from block store: ${error}", ("error", r.error_text) );
            },
            [&] ( const auto& r )
            {
               KOINOS_THROW( koinos::exception, "Unexpected response from block store" );
            }
         }, resp );
      }

      if ( !index_to )
      {
         LOG(info) << "Block application successful - Height: " << request.block.header.height << ", ID: " << request.block.id;
      }
      else if ( request.block.header.height % index_message_interval == 0 )
      {
         auto progress = request.block.header.height.t / static_cast< double >( index_to.t ) * 100;
         LOG(info) << "Indexing chain (" << progress << "%) - Height: " << request.block.header.height << ", ID: " << request.block.id;
      }

      auto output = ctx.get_pending_console_output();

      if ( output.length() > 0 )
      {
         LOG(info) << "Output:\n" << output;
      }

      auto lib = system_call::get_last_irreversible_block( ctx );

      _state_db.finalize_node( block_node->id() );

      if ( std::optional< state_node_ptr > node; lib > _state_db.get_root()->revision() )
      {
         node = _state_db.get_node_at_revision( uint64_t( lib ), block_node->id() );
         _state_db.commit_node( node.value()->id() );
      }

      const auto [ fork_heads, last_irreversible_block ] = get_fork_data_lockless();

      if ( _client && _client->is_connected() )
      {
         try
         {
            pack::json j;

            pack::to_json( j, broadcast::block_irreversible {
               .topology = last_irreversible_block
            } );

            _client->broadcast( "koinos.block.irreversible", j.dump() );
         }
         catch ( const std::exception& e )
         {
            LOG(error) << "Failed to publish block irreversible to message broker: " << e.what();
         }

         try
         {
            pack::json j;

            pack::to_json( j, broadcast::block_accepted {
               .block = request.block
            } );

            _client->broadcast( "koinos.block.accept", j.dump() );
         }
         catch ( const std::exception& e )
         {
            LOG(error) << "Failed to publish block application to message broker: " << e.what();
         }

         try
         {
            pack::json j;

            pack::to_json( j, broadcast::fork_heads {
               .fork_heads              = fork_heads,
               .last_irreversible_block = last_irreversible_block
            } );

            _client->broadcast( "koinos.block.forks", j.dump() );
         }
         catch ( const std::exception& e )
         {
            LOG(error) << "Failed to publish fork data to message broker: " << e.what();
         }
      }
   }
   catch( const std::exception& e )
   {
      LOG(warning) << "Block application failed - Height: " << request.block.header.height << " ID: " << request.block.id << ", with reason: " << e.what();

      auto output = ctx.get_pending_console_output();

      if ( output.length() > 0 )
      {
         LOG(info) << "Output:\n" << output;
      }

      if ( block_node )
      {
         _state_db.discard_node( block_node->id() );
      }

      throw;
   }
   catch( ... )
   {
      LOG(warning) << "Block application failed - Height: " << request.block.header.height << ", ID: " << request.block.id << ", for an unknown reason";

      auto output = ctx.get_pending_console_output();

      if ( output.length() > 0 )
      {
         LOG(info) << "Output:\n" << output;
      }

      if ( block_node )
      {
         _state_db.discard_node( block_node->id() );
      }

      throw;
   }

   return {};
}

rpc::chain::submit_transaction_response controller_impl::submit_transaction( const rpc::chain::submit_transaction_request& request )
{
   std::shared_lock< std::shared_mutex > lock( _state_db_mutex );

   koinos::protocol::account_type payer;
   uint128 max_payer_resources;
   uint128 trx_resource_limit;

   LOG(info) << "Pushing transaction - ID: " << request.transaction.id;

   auto pending_trx_node = _state_db.get_head()->create_anonymous_node();
   KOINOS_ASSERT( pending_trx_node, trx_state_error, "Error creating pending transaction state node" );

   apply_context ctx( _vm_backend );
   ctx.push_frame( stack_frame {
      .call = crypto::hash( CRYPTO_RIPEMD160_ID, "submit_transaction"s ).digest,
      .call_privilege = privilege::kernel_mode
   } );

   try
   {
      ctx.set_state_node( pending_trx_node );

      // The following call to set_meter_ticks() does not set an upper bound on the cycles that
      // can be used in the block.  Rather, it provides some initial cycles in case the subsequent
      // system calls are overridden.  See issue #439.

      ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);

      payer = system_call::get_transaction_payer( ctx, request.transaction );
      max_payer_resources = system_call::get_max_account_resources( ctx, payer );
      trx_resource_limit = system_call::get_transaction_resource_limit( ctx, request.transaction );

      system_call::apply_transaction( ctx, request.transaction );

      rpc::mempool::check_pending_account_resources_request check_req
      {
         .payer = payer,
         .max_payer_resources = max_payer_resources,
         .trx_resource_limit = trx_resource_limit
      };

      if ( _client && _client->is_connected() )
      {
         rpc::mempool::mempool_rpc_request mem_req = check_req;
         pack::json j;
         pack::to_json( j, mem_req );
         auto future = _client->rpc( service::mempool, j.dump(), 750 /* ms */, mq::retry_policy::none );

         rpc::mempool::mempool_rpc_response resp;
         pack::from_json( pack::json::parse( future.get() ), resp );

         std::visit( koinos::overloaded {
            [&]( const rpc::mempool::check_pending_account_resources_response& r )
            {
               if ( !r.success )
               {
                  KOINOS_THROW( koinos::exception, "Insufficient pending account resources" );
               }
            },
            [&] ( const rpc::mempool::mempool_error_response& r )
            {
               KOINOS_THROW( koinos::exception, r.error_text );
            },
            [&] ( const auto& r )
            {
               KOINOS_THROW( koinos::exception, "Unexpected response from mempool" );
            }
         }, resp );
      }

      LOG(info) << "Transaction application successful - ID: " << request.transaction.id;

      if ( _client && _client->is_connected() )
      {
         try
         {
            pack::json j;

            pack::to_json( j, broadcast::transaction_accepted {
               .transaction = request.transaction,
               .payer = payer,
               .max_payer_resources = max_payer_resources,
               .trx_resource_limit = trx_resource_limit,
               .height = block_height_type{ ctx.get_state_node()->revision() }
            } );

            _client->broadcast( "koinos.transaction.accept", j.dump() );
         }
         catch ( const std::exception& e )
         {
            LOG(error) << "Failed to publish block application to message broker: " << e.what();
         }
      }
   }
   catch( const std::exception& e )
   {
      LOG(warning) << "Transaction application failed - ID: " << request.transaction.id << ", with reason: " << e.what();
      throw;
   }
   catch( ... )
   {
      LOG(warning) << "Transaction application failed - ID: " << request.transaction.id << ", for an unknown reason";
      throw;
   }

   return {};
}

rpc::chain::get_head_info_response controller_impl::get_head_info( const rpc::chain::get_head_info_request& )
{
   std::shared_lock< std::shared_mutex > lock( _state_db_mutex );

   apply_context ctx( _vm_backend );
   ctx.push_frame( stack_frame {
      .call = crypto::hash( CRYPTO_RIPEMD160_ID, "get_head_info"s ).digest,
      .call_privilege = privilege::kernel_mode
   } );

   ctx.set_state_node( _state_db.get_head() );

   auto head_info = system_call::get_head_info( ctx );
   return {
      .head_topology            = head_info.head_topology,
      .last_irreversible_height = head_info.last_irreversible_height
   };
}

rpc::chain::get_chain_id_response controller_impl::get_chain_id( const rpc::chain::get_chain_id_request& )
{
   std::shared_lock< std::shared_mutex > lock( _state_db_mutex );

   boost::interprocess::basic_vectorstream< state_db::object_value > chain_id_stream;
   state_db::object_value chain_id_vector;
   chain_id_vector.resize( 128 );
   chain_id_stream.swap_vector( chain_id_vector );

   state_db::get_object_result result;
   state_db::get_object_args   args;
   args.space    = database::kernel_space;
   args.key      = database::key_from_string( database::key::chain_id );
   args.buf      = const_cast< char* >( chain_id_stream.vector().data() );
   args.buf_size = chain_id_stream.vector().size();

   state_db::state_node_ptr head;

   head = _state_db.get_head();

   head->get_object( result, args );

   KOINOS_ASSERT( result.key == args.key, retrieval_failure, "unable to retrieve chain id" );
   KOINOS_ASSERT( result.size <= args.buf_size, insufficent_buffer_size, "chain id buffer overflow" );

   multihash chain_id;
   pack::from_binary( chain_id_stream, chain_id, result.size );
   LOG(info) << "get_chain_id returning " << chain_id;

   return { .chain_id = chain_id };
}

fork_data controller_impl::get_fork_data()
{
   std::shared_lock< std::shared_mutex > lock( _state_db_mutex );
   return get_fork_data_lockless();
}

fork_data controller_impl::get_fork_data_lockless()
{
   fork_data fdata;
   apply_context ctx( _vm_backend );

   ctx.push_frame( koinos::chain::stack_frame {
      .call = crypto::hash( CRYPTO_RIPEMD160_ID, "get_fork_data"s ).digest,
      .call_privilege = privilege::kernel_mode
   } );

   std::vector< state_db::state_node_ptr > fork_heads;

   ctx.set_state_node( _state_db.get_root() );
   fork_heads = _state_db.get_fork_heads();

   auto head_info = system_call::get_head_info( ctx );
   fdata.second = head_info.head_topology;

   for ( auto& fork : fork_heads )
   {
      ctx.set_state_node( fork );
      auto head_info = system_call::get_head_info( ctx );
      fdata.first.emplace_back( std::move( head_info.head_topology ) );
   }

   // Sort all fork heads by height
   std::sort( fdata.first.begin(), fdata.first.end(), []( const block_topology& a, const block_topology& b )
   {
      return a.height > b.height;
   } );

   // If there is a tie for highest block, ensure the head block is first
   auto fork_itr = fdata.first.begin();
   while ( fork_itr != fdata.first.begin() && fork_itr->id != head_info.head_topology.id )
   {
      ++fork_itr;
   }

   if ( fork_itr != fdata.first.begin() )
   {
      std::iter_swap( fork_itr, fdata.first.begin() );
   }

   return fdata;
}

rpc::chain::get_fork_heads_response controller_impl::get_fork_heads( const rpc::chain::get_fork_heads_request& )
{
   rpc::chain::get_fork_heads_response response;

   const auto [ fork_heads, last_irreversible_block ] = get_fork_data();

   response.fork_heads              = fork_heads;
   response.last_irreversible_block = last_irreversible_block;

   return response;
}

rpc::chain::read_contract_response controller_impl::read_contract( const rpc::chain::read_contract_request& request )
{
   std::shared_lock< std::shared_mutex > lock( _state_db_mutex );

   state_db::state_node_ptr head_node;

   head_node = _state_db.get_head();

   apply_context ctx( _vm_backend );
   ctx.push_frame( stack_frame {
      .call = crypto::hash( CRYPTO_RIPEMD160_ID, "read_contract"s ).digest,
      .call_privilege = privilege::kernel_mode,
   } );

   ctx.set_state_node( head_node );
   ctx.set_read_only( true );
   ctx.set_meter_ticks( _max_read_cycles );

   auto result = system_call::execute_contract( ctx, request.contract_id, request.entry_point, request.args );

   return rpc::chain::read_contract_response {
      .result = std::move( result ),
      .logs = ctx.get_pending_console_output()
   };
}

rpc::chain::get_account_nonce_response controller_impl::get_account_nonce( const rpc::chain::get_account_nonce_request& request )
{
   apply_context ctx( _vm_backend );

   ctx.push_frame( koinos::chain::stack_frame {
      .call = crypto::hash( CRYPTO_RIPEMD160_ID, "get_account_nonce"s ).digest,
      .call_privilege = privilege::kernel_mode
   } );

   std::shared_lock< std::shared_mutex > lock( _state_db_mutex );

   ctx.set_state_node( _state_db.get_head() );
   return { system_call::get_account_nonce( ctx, request.account ).nonce };
}

} // detail

controller::controller() : _my( std::make_unique< detail::controller_impl >() ) {}

controller::~controller() = default;

void controller::open( const std::filesystem::path& p, const std::any& o, const genesis_data& data, bool reset )
{
   _my->open( p, o, data, reset );
}

void controller::set_client( std::shared_ptr< mq::client > c )
{
   _my->set_client( c );
}

rpc::chain::submit_block_response controller::submit_block(
   const rpc::chain::submit_block_request& request,
   block_height_type index_to,
   std::chrono::system_clock::time_point now )
{
   return _my->submit_block( request, index_to, now );
}

rpc::chain::submit_transaction_response controller::submit_transaction( const rpc::chain::submit_transaction_request& request )
{
   return _my->submit_transaction( request );
}

rpc::chain::get_head_info_response controller::get_head_info( const rpc::chain::get_head_info_request& request )
{
   return _my->get_head_info( request );
}

rpc::chain::get_chain_id_response controller::get_chain_id( const rpc::chain::get_chain_id_request& request )
{
   return _my->get_chain_id( request );
}

rpc::chain::get_fork_heads_response controller::get_fork_heads( const rpc::chain::get_fork_heads_request& request )
{
   return _my->get_fork_heads( request );
}

rpc::chain::read_contract_response controller::read_contract( const rpc::chain::read_contract_request& request )
{
   return _my->read_contract( request );
}

rpc::chain::get_account_nonce_response controller::get_account_nonce( const rpc::chain::get_account_nonce_request& request )
{
   return _my->get_account_nonce( request );
}

} // koinos::chain
