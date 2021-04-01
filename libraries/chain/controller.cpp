#include <koinos/chain/controller.hpp>

#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/host.hpp>
#include <koinos/chain/system_calls.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/json.hpp>

#include <koinos/statedb/statedb.hpp>

#include <koinos/util.hpp>

#include <mira/database_configuration.hpp>

#include <algorithm>
#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace koinos::chain {

using namespace std::string_literals;

using vectorstream = boost::interprocess::basic_vectorstream< std::vector< char > >;

namespace detail {

class controller_impl final
{
   public:
      controller_impl();
      ~controller_impl();

      void open( const boost::filesystem::path& p, const std::any& o, const genesis_data& data, bool reset );
      void set_client( std::shared_ptr< mq::client > c );

      rpc::chain::submit_block_response       submit_block(       const rpc::chain::submit_block_request&, bool indexing );
      rpc::chain::submit_transaction_response submit_transaction( const rpc::chain::submit_transaction_request& );
      rpc::chain::get_head_info_response      get_head_info(      const rpc::chain::get_head_info_request&      );
      rpc::chain::get_chain_id_response       get_chain_id(       const rpc::chain::get_chain_id_request&      );
      rpc::chain::get_fork_heads_response     get_fork_heads(     const rpc::chain::get_fork_heads_request&     );

   private:
      statedb::state_db             _state_db;
      std::mutex                    _state_db_mutex;
      std::shared_ptr< mq::client > _client;
};

controller_impl::controller_impl()
{
   register_host_functions();
}

controller_impl::~controller_impl()
{
   std::lock_guard< std::mutex > lock( _state_db_mutex );
   _state_db.close();
}

void controller_impl::open( const boost::filesystem::path& p, const std::any& o, const genesis_data& data, bool reset )
{
   std::lock_guard< std::mutex > lock( _state_db_mutex );
   _state_db.open( p, o, [&]( statedb::state_node_ptr root )
   {
      for ( const auto& entry : data )
      {
         statedb::put_object_args put_args;
         put_args.space = 0;
         put_args.key = entry.first;
         put_args.buf = entry.second.data();
         put_args.object_size = entry.second.size();

         statedb::put_object_result put_res;
         root->put_object( put_res, put_args );

         KOINOS_ASSERT(
            !put_res.object_existed,
            koinos::chain::database_exception,
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
   LOG(info) << "Opened database at block - height: " << head->revision() << ", id: " << head->id();
}

void controller_impl::set_client( std::shared_ptr< mq::client > c )
{
   _client = c;
}

rpc::chain::submit_block_response controller_impl::submit_block( const rpc::chain::submit_block_request& request, bool indexing )
{
   if( crypto::multihash_is_zero( request.block.header.previous ) )
   {
      // Genesis case
      KOINOS_ASSERT( request.block.header.height == 1, root_height_mismatch, "First block must have height of 1" );
   }

   // Check if the block has already been applied
   statedb::state_node_ptr block_node;

   {
      std::lock_guard< std::mutex > lock( _state_db_mutex );
      block_node = _state_db.get_node( request.block.id );

      if ( block_node ) return {}; // Block has been applied

      if ( !indexing || request.block.header.height % 10000 == 0 )
      {
         LOG(info) << "Applying block - Height: " << request.block.header.height
            << ", ID: " << request.block.id;
      }
      block_node = _state_db.create_writable_node( request.block.header.previous, request.block.id );
      KOINOS_ASSERT( block_node, unknown_previous_block, "Unknown previous block" );
   }

   try
   {
      apply_context ctx;
      ctx.push_frame( stack_frame {
         .call = pack::to_variable_blob( "submit_block"s ),
         .call_privilege = privilege::kernel_mode
      } );

      ctx.set_state_node( block_node );

      system_call::apply_block(
         ctx,
         request.block,
         request.verify_passive_data,
         request.verify_block_signature,
         request.verify_transaction_signatures );
      auto output = ctx.get_pending_console_output();

      if ( output.length() > 0 ) { LOG(info) << output; }

      auto lib = system_call::get_last_irreversible_block( ctx );

      std::lock_guard< std::mutex > lock( _state_db_mutex );
      _state_db.finalize_node( block_node->id() );

      if ( lib > _state_db.get_root()->revision() )
      {
         auto node = _state_db.get_node_at_revision( uint64_t( lib ), block_node->id() );
         _state_db.commit_node( node->id() );

         if ( _client && _client->is_connected() )
         {
            try
            {
                broadcast::block_irreversible msg {
                  .topology = {
                     .id       = node->id(),
                     .height   = block_height_type( node->revision() ),
                     .previous = node->parent_id()
                  }
               };

               pack::json j;

               pack::to_json( j, msg );

               _client->broadcast( "koinos.block.irreversible", j.dump() );
            }
            catch ( const std::exception& e )
            {
               LOG(error) << "Failed to publish block irreversible to message broker: " << e.what();
            }
         }
      }
   }
   catch( const koinos::exception& )
   {
      _state_db.discard_node( block_node->id() );
      throw;
   }

   if ( _client && _client->is_connected() )
   {
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
   }

   return {};
}

rpc::chain::submit_transaction_response controller_impl::submit_transaction( const rpc::chain::submit_transaction_request& request )
{
   const multihash tmp_id = multihash {
      .id = CRYPTO_SHA2_256_ID,
      .digest = { 1 }
   };

   koinos::chain::account_type payer;
   uint128 max_payer_resources;
   uint128 trx_resource_limit;

   LOG(info) << "Applying transaction - id: " << request.transaction.id;

   statedb::state_node_ptr pending_trx_node;

   {
      std::lock_guard< std::mutex > lock( _state_db_mutex );
      pending_trx_node = _state_db.get_node( request.transaction.id );

      KOINOS_ASSERT( !pending_trx_node, duplicate_trx_state, "Pending transaction is already being applied" );

      LOG(info) << "Applying transaction - ID: " << request.transaction.id;
      pending_trx_node = _state_db.create_writable_node( _state_db.get_head()->id(), request.transaction.id );
      KOINOS_ASSERT( pending_trx_node, trx_state_error, "Error creating pending transaction state node" );
   }

   apply_context ctx;
   ctx.push_frame( stack_frame {
      .call = pack::to_variable_blob( "submit_transaction"s ),
      .call_privilege = privilege::kernel_mode
   } );

   try
   {
      ctx.set_state_node( pending_trx_node );

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
         pack::to_json( j, check_req );
         auto future = _client->rpc( mq::service::mempool, j.dump() );

         rpc::mempool::mempool_rpc_response resp;
         pack::from_json( pack::json::parse( future.get() ), resp );

         std::visit( koinos::overloaded {
            [&]( const rpc::mempool::check_pending_account_resources_response& r )
            {
               if ( !r.success )
               {
                  throw koinos::exception( "Insufficient pending account resources" );
               }
            },
            [&] ( const rpc::mempool::mempool_error_response& r )
            {
               throw koinos::exception( r.error_text );
            },
            [&] ( const auto& r )
            {
               throw koinos::exception( "Unexpected response from mempool" );
            }
         }, resp );
      }

      std::lock_guard< std::mutex > lock( _state_db_mutex );
      _state_db.discard_node( tmp_id );
   }
   catch( const koinos::exception& )
   {
      std::lock_guard< std::mutex > lock( _state_db_mutex );
      _state_db.discard_node( tmp_id );
      throw;
   }

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

   return {};
}

rpc::chain::get_head_info_response controller_impl::get_head_info( const rpc::chain::get_head_info_request& )
{
   apply_context ctx;
   ctx.push_frame( stack_frame {
      .call = pack::to_variable_blob( "get_head_info"s ),
      .call_privilege = privilege::kernel_mode
   } );

   {
      std::lock_guard< std::mutex > lock( _state_db_mutex );
      ctx.set_state_node( _state_db.get_head() );
   }

   auto head_info = system_call::get_head_info( ctx );
   return {
      .head_topology            = head_info.head_topology,
      .last_irreversible_height = head_info.last_irreversible_height
   };
}

rpc::chain::get_chain_id_response controller_impl::get_chain_id( const rpc::chain::get_chain_id_request& )
{
   boost::interprocess::basic_vectorstream< statedb::object_value > chain_id_stream;
   statedb::object_value chain_id_vector;
   chain_id_vector.resize( 128 );
   chain_id_stream.swap_vector( chain_id_vector );

   statedb::get_object_result result;
   statedb::get_object_args   args;
   args.space    = KOINOS_STATEDB_SPACE;
   args.key      = KOINOS_STATEDB_CHAIN_ID_KEY;
   args.buf      = const_cast< char* >( chain_id_stream.vector().data() );
   args.buf_size = chain_id_stream.vector().size();

   statedb::state_node_ptr head;

   {
      std::lock_guard< std::mutex > lock( _state_db_mutex );
      head = _state_db.get_head();
   }

   head->get_object( result, args );

   KOINOS_ASSERT( result.key == args.key, database_exception, "unable to retrieve chain id" );
   KOINOS_ASSERT( result.size <= args.buf_size, database_exception, "chain id buffer overflow" );

   multihash chain_id;
   pack::from_binary( chain_id_stream, chain_id, result.size );
   LOG(info) << "get_chain_id returning " << chain_id;

   return { .chain_id = chain_id };
}

rpc::chain::get_fork_heads_response controller_impl::get_fork_heads( const rpc::chain::get_fork_heads_request& )
{
   rpc::chain::get_fork_heads_response response;

   apply_context ctx;
   ctx.push_frame( koinos::chain::stack_frame {
      .call = pack::to_variable_blob( "get_fork_heads"s ),
      .call_privilege = privilege::kernel_mode
   } );

   std::vector< statedb::state_node_ptr > fork_heads;

   {
      std::lock_guard< std::mutex > lock( _state_db_mutex );
      ctx.set_state_node( _state_db.get_root() );
      fork_heads = _state_db.get_fork_heads();
   }

   response.last_irreversible_block = system_call::get_head_info( ctx ).head_topology;

   for( auto& fork : fork_heads )
   {
      ctx.set_state_node( fork );
      auto head_info = system_call::get_head_info( ctx );
      response.fork_heads.emplace_back( std::move( head_info.head_topology ) );
   }

   return response;
}

} // detail

controller::controller() : _my( std::make_unique< detail::controller_impl >() ) {}

controller::~controller() = default;

void controller::open( const boost::filesystem::path& p, const std::any& o, const genesis_data& data, bool reset )
{
   _my->open( p, o, data, reset );
}

void controller::set_client( std::shared_ptr< mq::client > c )
{
   _my->set_client( c );
}

rpc::chain::submit_block_response controller::submit_block( const rpc::chain::submit_block_request& request, bool indexing )
{
   return _my->submit_block( request, indexing );
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

} // koinos::chain
