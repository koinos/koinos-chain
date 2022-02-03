#include <koinos/block_store/block_store.pb.h>
#include <koinos/broadcast/broadcast.pb.h>

#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/constants.hpp>
#include <koinos/chain/controller.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/pending_state.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/system_calls.hpp>

#include <koinos/rpc/block_store/block_store_rpc.pb.h>
#include <koinos/rpc/chain/chain_rpc.pb.h>
#include <koinos/rpc/mempool/mempool_rpc.pb.h>

#include <koinos/state_db/state_db.hpp>

#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>
#include <koinos/util/services.hpp>

#include <koinos/vm_manager/vm_backend.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <list>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <thread>

#include <boost/interprocess/streams/vectorstream.hpp>

namespace koinos::chain {

using namespace std::string_literals;
using namespace std::chrono_literals;

using vectorstream = boost::interprocess::basic_vectorstream< std::vector< char > >;
using fork_data    = std::pair< std::vector< block_topology >, block_topology >;

namespace detail {

class controller_impl final
{
   public:
      controller_impl( uint64_t read_compute_bandwith_limit );
      ~controller_impl();

      void open( const std::filesystem::path& p, const genesis_data& data, bool reset );
      void set_client( std::shared_ptr< mq::client > c );

      rpc::chain::submit_block_response submit_block(
         const rpc::chain::submit_block_request&,
         uint64_t index_to,
         std::chrono::system_clock::time_point now
      );

      rpc::chain::submit_transaction_response submit_transaction( const rpc::chain::submit_transaction_request& );
      rpc::chain::get_head_info_response get_head_info( const rpc::chain::get_head_info_request& );
      rpc::chain::get_chain_id_response get_chain_id( const rpc::chain::get_chain_id_request& );
      rpc::chain::get_fork_heads_response get_fork_heads( const rpc::chain::get_fork_heads_request& );
      rpc::chain::read_contract_response read_contract( const rpc::chain::read_contract_request& );
      rpc::chain::get_account_nonce_response get_account_nonce( const rpc::chain::get_account_nonce_request& );
      rpc::chain::get_account_rc_response get_account_rc( const rpc::chain::get_account_rc_request& );
      rpc::chain::get_resource_limits_response get_resource_limits( const rpc::chain::get_resource_limits_request& );

   private:
      state_db::database                        _db;
      std::shared_mutex                         _db_mutex;
      std::shared_ptr< vm_manager::vm_backend > _vm_backend;
      std::shared_ptr< mq::client >             _client;
      pending_state                             _pending_state;
      uint64_t                                  _read_compute_bandwidth_limit;

      void validate_block( const protocol::block& b );
      void validate_transaction( const protocol::transaction& t );

      fork_data get_fork_data();
      fork_data get_fork_data_lockless();
};

controller_impl::controller_impl( uint64_t read_compute_bandwidth_limit ) : _read_compute_bandwidth_limit( read_compute_bandwidth_limit )
{
   _vm_backend = vm_manager::get_vm_backend(); // Default is fizzy
   KOINOS_ASSERT( _vm_backend, unknown_backend_exception, "could not get vm backend" );

   _vm_backend->initialize();
   LOG(info) << "Initialized " << _vm_backend->backend_name() << " vm backend";

   _pending_state.set_backend( _vm_backend );
}

controller_impl::~controller_impl()
{
   std::lock_guard< std::shared_mutex > lock( _db_mutex );
   _db.close();
}

void controller_impl::open( const std::filesystem::path& p, const chain::genesis_data& data, bool reset )
{
   std::lock_guard< std::shared_mutex > lock( _db_mutex );

   _db.open( p, [&]( state_db::state_node_ptr root )
   {
      // Write genesis objects into the database
      for ( const auto& entry : data.entries() )
      {
         KOINOS_ASSERT(
            root->put_object( entry.space(), entry.key(), &entry.value() ) == entry.value().size(),
            koinos::chain::unexpected_state,
            "encountered unexpected object in initial state"
         );
      }
      LOG(info) << "Wrote " << data.entries().size() << " genesis objects into new database";

      // Read genesis public key from the database, assert its existence at the correct location
      KOINOS_ASSERT(
         root->get_object( state::space::metadata(), state::key::genesis_key ),
         koinos::chain::unexpected_state,
         "could not find genesis public key in database"
      );

      // Calculate and write the chain ID into the database
      auto chain_id = crypto::hash( koinos::crypto::multicodec::sha2_256, data );
      LOG(info) << "Calculated chain ID: " << chain_id;
      auto chain_id_str = util::converter::as< std::string >( chain_id );
      KOINOS_ASSERT(
         root->put_object( chain::state::space::metadata(), chain::state::key::chain_id, &chain_id_str ) == chain_id_str.size(),
         koinos::chain::unexpected_state,
         "encountered unexpected chain id in initial state"
      );
      LOG(info) << "Wrote chain ID into new database";
   } );

   if ( reset )
   {
      LOG(info) << "Resetting database...";
      _db.reset();
   }

   auto head = _db.get_head();
   _pending_state.rebuild( head );
   LOG(info) << "Opened database at block - Height: " << head->revision() << ", ID: " << head->id();
}

void controller_impl::set_client( std::shared_ptr< mq::client > c )
{
   _client = c;
   _pending_state.set_client( c );
}

void controller_impl::validate_block( const protocol::block& b )
{
   KOINOS_ASSERT( b.id().size(), missing_required_arguments, "missing expected field in block: ${field}", ("field", "id") );
   KOINOS_ASSERT( b.has_header(), missing_required_arguments, "missing expected field in block: ${field}", ("field", "header")("block_id", util::to_hex( b.id() )) );
   KOINOS_ASSERT( b.header().previous().size(), missing_required_arguments, "missing expected field in block header: ${field}", ("field", "previous")("block_id", util::to_hex( b.id() )) );
   KOINOS_ASSERT( b.header().height(), missing_required_arguments, "missing expected field in block header: ${field}", ("field", "height")("block_id", util::to_hex( b.id() )) );
   KOINOS_ASSERT( b.header().timestamp(), missing_required_arguments, "missing expected field in block header: ${field}", ("field", "timestamp")("block_id", util::to_hex( b.id() )) );
   KOINOS_ASSERT( b.header().previous_state_merkle_root().size(), missing_required_arguments, "missing expected field in block header: ${field}", ("field", "previous_state_merkle_root")("block_id", util::to_hex( b.id() )) );
   KOINOS_ASSERT( b.header().transaction_merkle_root().size(), missing_required_arguments, "missing expected field in block header: ${field}", ("field", "transaction_merkle_root")("block_id", util::to_hex( b.id() )) );
   KOINOS_ASSERT( b.signature().size(), missing_required_arguments, "missing expected field in block: ${field}", ("field", "signature_data")("block_id", util::to_hex( b.id() )) );

   for ( const auto& t : b.transactions() )
      validate_transaction( t );
}

void controller_impl::validate_transaction( const protocol::transaction& t )
{
   KOINOS_ASSERT( t.id().size(), missing_required_arguments, "missing expected field in transaction: ${field}", ("field", "id") );
   KOINOS_ASSERT( t.has_header(), missing_required_arguments, "missing expected field in transaction: ${field}", ("field", "header")("transaction_id", util::to_hex( t.id() )) );
   KOINOS_ASSERT( t.header().rc_limit(), missing_required_arguments, "missing expected field in transaction header: ${field}", ("field", "rc_limit")("transaction_id", util::to_hex( t.id() )) );
   KOINOS_ASSERT( t.header().operation_merkle_root().size(), missing_required_arguments, "missing expected field in transaction header: ${field}", ("field", "operation_merkle_root")("transaction_id", util::to_hex( t.id() )) );
   KOINOS_ASSERT( t.signatures().size(), missing_required_arguments, "missing expected field in transaction: ${field}", ("field", "signature_data")("transaction_id", util::to_hex( t.id() )) );
}

rpc::chain::submit_block_response controller_impl::submit_block(
   const rpc::chain::submit_block_request& request,
   uint64_t index_to,
   std::chrono::system_clock::time_point now )
{
   std::lock_guard< std::shared_mutex > lock( _db_mutex );

   validate_block( request.block() );

   rpc::chain::submit_block_response resp;

   static constexpr uint64_t index_message_interval = 1000;
   static constexpr std::chrono::seconds time_delta = std::chrono::seconds( 5 );

   auto time_lower_bound  = uint64_t( 0 );
   auto time_upper_bound  = std::chrono::duration_cast< std::chrono::milliseconds >( ( now + time_delta ).time_since_epoch() ).count();
   uint64_t parent_height = 0;

   state_db::state_node_ptr block_node;

   auto block        = request.block();
   auto block_id     = util::converter::to< crypto::multihash >( block.id() );
   auto block_height = block.header().height();
   auto parent_id    = util::converter::to< crypto::multihash >( block.header().previous() );
   block_node        = _db.get_node( block_id );
   auto parent_node  = _db.get_node( parent_id );

   if ( block_node ) return {}; // Block has been applied

   if ( !index_to )
   {
      LOG(info) << "Pushing block - Height: " << block_height << ", ID: " << block_id;
   }

   block_node = _db.create_writable_node( parent_id, block_id );

   // If this is not the genesis case, we must ensure that the proposed block timestamp is greater
   // than the parent block timestamp.
   if ( block_node && !parent_id.is_zero() )
   {
      execution_context parent_ctx( _vm_backend, intent::read_only );

      parent_ctx.push_frame( stack_frame {
         .call_privilege = privilege::kernel_mode
      } );

      parent_ctx.set_state_node( parent_node );
      parent_ctx.build_cache();
      auto head_info = system_call::get_head_info( parent_ctx );
      parent_height = head_info.head_topology().height();
      time_lower_bound = head_info.head_block_time();
   }

   execution_context ctx( _vm_backend, intent::block_application );

   try
   {
      // Genesis case, when the first block is submitted the previous must be the zero hash
      if ( parent_id.is_zero() )
      {
         KOINOS_ASSERT( block_height == 1, unexpected_height, "first block must have height of 1" );
      }

      KOINOS_ASSERT(
         crypto::hash( crypto::multicodec::sha2_256, block.header() ) == block_id,
         block_id_mismatch,
         "block contains an invalid block ID"
      );

      KOINOS_ASSERT( block_node, unknown_previous_block, "unknown previous block" );

      KOINOS_ASSERT(
         block_height == parent_height + 1,
         unexpected_height,
         "expected block height of ${a}, was ${b}", ("a", parent_height + 1)("b", block_height)
      );

      KOINOS_ASSERT( block.header().timestamp() <= time_upper_bound, timestamp_out_of_bounds, "block timestamp is too far in the future" );
      KOINOS_ASSERT( block.header().timestamp() >= time_lower_bound, timestamp_out_of_bounds, "block timestamp is too old" );

      KOINOS_ASSERT(
         block.header().previous_state_merkle_root() == util::converter::as< std::string >( parent_node->get_merkle_root() ),
         state_merkle_mismatch,
         "block previous state merkle mismatch"
      );

      ctx.push_frame( stack_frame {
         .call_privilege = privilege::kernel_mode
      } );

      ctx.set_state_node( block_node );
      ctx.build_cache();

      system_call::apply_block( ctx, block );

      if ( _client && _client->is_running() )
      {
         rpc::block_store::block_store_request req;
         req.mutable_add_block()->mutable_block_to_add()->CopyFrom( block );

         auto future = _client->rpc( util::service::block_store, util::converter::as< std::string >( req ), 750ms, mq::retry_policy::none );

         rpc::block_store::block_store_response resp;
         resp.ParseFromString( future.get() );

         KOINOS_ASSERT( !resp.has_error(), rpc_failure, "received error from block store: ${e}", ("e", resp.error()) );
         KOINOS_ASSERT( resp.has_add_block(), rpc_failure, "unexpected response when submitting block: ${r}", ("r", resp) );
      }

      uint64_t disk_storage_used      = ctx.resource_meter().disk_storage_used();
      uint64_t network_bandwidth_used = ctx.resource_meter().network_bandwidth_used();
      uint64_t compute_bandwidth_used = ctx.resource_meter().compute_bandwidth_used();

      KOINOS_ASSERT( std::holds_alternative< protocol::block_receipt >( ctx.receipt() ), unexpected_receipt, "expected block receipt" );
      *resp.mutable_receipt() = std::get< protocol::block_receipt >( ctx.receipt() );

      if ( !index_to )
      {
         auto num_transactions = block.transactions_size();

         LOG(info) << "Block application successful - Height: " << block_height << ", ID: " << block_id << " (" << num_transactions << ( num_transactions == 1 ? " transaction)" : " transactions)" );
         LOG(info) << "Consumed resources: " << disk_storage_used << " disk, " << network_bandwidth_used << " network, " << compute_bandwidth_used << " compute";
      }
      else if ( block_height % index_message_interval == 0 )
      {
         auto progress = block_height / static_cast< double >( index_to ) * 100;
         LOG(info) << "Indexing chain (" << progress << "%) - Height: " << block_height << ", ID: " << block_id;
      }

      auto lib = system_call::get_last_irreversible_block( ctx );

      _db.finalize_node( block_node->id() );

      if ( std::optional< state_node_ptr > node; lib > _db.get_root()->revision() )
      {
         node = _db.get_node_at_revision( lib, block_node->id() );
         _db.commit_node( node.value()->id() );
      }

      resp.mutable_receipt()->set_state_merkle_root( util::converter::as< std::string >( block_node->get_merkle_root() ) );

      const auto [ fork_heads, last_irreversible_block ] = get_fork_data_lockless();

      if ( _client && _client->is_running() )
      {
         try
         {
            broadcast::block_irreversible bc;
            bc.mutable_topology()->CopyFrom( last_irreversible_block );

            _client->broadcast( "koinos.block.irreversible", util::converter::as< std::string >( bc ) );
         }
         catch ( const std::exception& e )
         {
            LOG(error) << "Failed to publish block irreversible to message broker: " << e.what();
         }

         try
         {
            broadcast::block_accepted ba;
            *ba.mutable_block() = block;
            *ba.mutable_receipt() = std::get< protocol::block_receipt >( ctx.receipt() );

            _client->broadcast( "koinos.block.accept", util::converter::as< std::string >( ba ) );
         }
         catch ( const std::exception& e )
         {
            LOG(error) << "Failed to publish block application to message broker: " << e.what();
         }

         try
         {
            broadcast::fork_heads fh;
            fh.mutable_last_irreversible_block()->CopyFrom( last_irreversible_block );

            for ( const auto& fork_head : fork_heads )
            {
               auto* head = fh.add_heads();
               *head = fork_head;
            }

            _client->broadcast( "koinos.block.forks", util::converter::as< std::string >( fh ) );
         }
         catch ( const std::exception& e )
         {
            LOG(error) << "Failed to publish fork data to message broker: " << e.what();
         }

         try
         {
            for ( const auto& [ unused, event ] : ctx.chronicler().events() )
            {
               _client->broadcast( "koinos.event." + util::to_base58( event.source() ) + "." + event.name(), event.data() );
            }
         }
         catch ( const std::exception& e )
         {
            LOG(error) << "Failed to publish block and transaction events to message broker: " << e.what();
         }
      }

      try
      {
         if ( block_node == _db.get_head() )
         {
            _pending_state.rebuild( block_node );
         }
      }
      catch ( const std::exception& e )
      {
         LOG(error) << "Failed to rebuild pending state: " << e.what();
      }
   }
   catch ( const koinos::exception& e )
   {
      LOG(warning) << "Block application failed - Height: " << block_height << " ID: " << block_id << ", with reason: " << e.what();

      if ( _client && _client->is_running() )
      {
         auto exception_data = e.get_json();

         if ( exception_data.count( "transaction_id" ) )
         {
            broadcast::transaction_failed ptf;
            ptf.set_id( exception_data[ "transaction_id" ] );
            _client->broadcast( "koinos.transaction.fail", util::converter::as< std::string >( ptf ) );
         }
      }

      if ( block_node )
      {
         _db.discard_node( block_node->id() );
      }

      throw;
   }
   catch ( ... )
   {
      LOG(warning) << "Block application failed - Height: " << block_height << ", ID: " << block_id << ", for an unknown reason";

      if ( block_node )
      {
         _db.discard_node( block_node->id() );
      }

      throw;
   }

   return resp;
}

rpc::chain::submit_transaction_response controller_impl::submit_transaction( const rpc::chain::submit_transaction_request& request )
{
   std::shared_lock< std::shared_mutex > lock( _db_mutex );

   validate_transaction( request.transaction() );

   rpc::chain::submit_transaction_response resp;

   std::string payer;
   uint64_t max_payer_rc;
   uint64_t trx_rc_limit;

   auto transaction    = request.transaction();
   auto transaction_id = util::to_hex( transaction.id() );

   LOG(info) << "Pushing transaction - ID: " << transaction_id;

   auto pending_trx_node = _pending_state.get_state_node();
   KOINOS_ASSERT( pending_trx_node, pending_state_error, "error retrieving pending state node" );

   execution_context ctx( _vm_backend, intent::transaction_application );

   ctx.push_frame( stack_frame {
      .call_privilege = privilege::kernel_mode
   } );

   try
   {
      ctx.set_state_node( pending_trx_node );
      ctx.build_cache();

      payer = transaction.header().payer();

      max_payer_rc = system_call::get_account_rc( ctx, payer );

      trx_rc_limit = transaction.header().rc_limit();

      ctx.resource_meter().set_resource_limit_data( system_call::get_resource_limits( ctx ) );

      system_call::apply_transaction( ctx, transaction );

      uint64_t disk_storage_used      = ctx.resource_meter().disk_storage_used();
      uint64_t network_bandwidth_used = ctx.resource_meter().network_bandwidth_used();
      uint64_t compute_bandwidth_used = ctx.resource_meter().compute_bandwidth_used();

      if ( _client && _client->is_running() )
      {
         rpc::mempool::mempool_request req;
         auto* check_pending = req.mutable_check_pending_account_resources();

         check_pending->set_payer( payer );
         check_pending->set_max_payer_rc( max_payer_rc );
         check_pending->set_rc_limit( trx_rc_limit );

         auto future = _client->rpc( util::service::mempool, util::converter::as< std::string >( req ), 750ms, mq::retry_policy::none );

         rpc::mempool::mempool_response resp;
         resp.ParseFromString( future.get() );

         KOINOS_ASSERT( !resp.has_error(), rpc_failure, "received error from mempool: ${e}", ("e", resp.error()) );
         KOINOS_ASSERT( resp.has_check_pending_account_resources(), rpc_failure, "received unexpected response from mempool" );
      }

      LOG(info) << "Transaction application successful - ID: " << transaction_id;

      KOINOS_ASSERT( std::holds_alternative< protocol::transaction_receipt >( ctx.receipt() ), unexpected_receipt, "expected transaction receipt" );
      *resp.mutable_receipt() = std::get< protocol::transaction_receipt >( ctx.receipt() );

      if ( _client && _client->is_running() )
      {
         try
         {
            broadcast::transaction_accepted ta;
            *ta.mutable_transaction() = transaction;
            *ta.mutable_receipt() = std::get< protocol::transaction_receipt >( ctx.receipt() );
            ta.set_height( ctx.get_state_node()->revision() );

            _client->broadcast( "koinos.transaction.accept", util::converter::as< std::string >( ta ) );
         }
         catch ( const std::exception& e )
         {
            LOG(error) << "Failed to publish block application to message broker: " << e.what();
         }
      }
   }
   catch( const std::exception& e )
   {
      LOG(warning) << "Transaction application failed - ID: " << transaction_id << ", with reason: " << e.what();
      throw;
   }
   catch( ... )
   {
      LOG(warning) << "Transaction application failed - ID: " << transaction_id << ", for an unknown reason";
      throw;
   }

   return resp;
}

rpc::chain::get_head_info_response controller_impl::get_head_info( const rpc::chain::get_head_info_request& )
{
   std::shared_lock< std::shared_mutex > lock( _db_mutex );

   execution_context ctx( _vm_backend );
   ctx.push_frame( stack_frame {
      .call_privilege = privilege::kernel_mode
   } );

   ctx.set_state_node( _db.get_head() );
   ctx.build_cache();

   auto head_info = system_call::get_head_info( ctx );
   block_topology topo = head_info.head_topology();

   rpc::chain::get_head_info_response resp;
   *resp.mutable_head_topology() = topo;
   resp.set_last_irreversible_block( head_info.last_irreversible_block() );
   resp.set_head_state_merkle_root( util::converter::as< std::string >( ctx.get_state_node()->get_merkle_root() ) );

   return resp;
}

rpc::chain::get_chain_id_response controller_impl::get_chain_id( const rpc::chain::get_chain_id_request& )
{
   std::shared_lock< std::shared_mutex > lock( _db_mutex );

   auto result = _db.get_head()->get_object( state::space::metadata(), state::key::chain_id );

   KOINOS_ASSERT( result, retrieval_failure, "unable to retrieve chain id" );

   LOG(debug) << "get_chain_id: " << util::converter::to< crypto::multihash >( *result );

   rpc::chain::get_chain_id_response resp;
   resp.set_chain_id( result->data(), result->size() );

   return resp;
}

fork_data controller_impl::get_fork_data()
{
   std::shared_lock< std::shared_mutex > lock( _db_mutex );
   return get_fork_data_lockless();
}

fork_data controller_impl::get_fork_data_lockless()
{
   fork_data fdata;
   execution_context ctx( _vm_backend );

   ctx.push_frame( koinos::chain::stack_frame {
      .call_privilege = privilege::kernel_mode
   } );

   std::vector< state_db::state_node_ptr > fork_heads;

   ctx.set_state_node( _db.get_root() );
   ctx.build_cache();
   fork_heads = _db.get_fork_heads();

   auto head_info = system_call::get_head_info( ctx );
   fdata.second = head_info.head_topology();

   for ( auto& fork : fork_heads )
   {
      ctx.set_state_node( fork );
      auto head_info = system_call::get_head_info( ctx );
      fdata.first.emplace_back( std::move( head_info.head_topology() ) );
   }

   // Sort all fork heads by height
   std::sort( fdata.first.begin(), fdata.first.end(), []( const block_topology& a, const block_topology& b )
   {
      return a.height() > b.height();
   } );

   // If there is a tie for highest block, ensure the head block is first
   auto fork_itr = fdata.first.begin();
   while ( fork_itr != fdata.first.begin() && fork_itr->id() != head_info.head_topology().id() )
   {
      ++fork_itr;
   }

   if ( fork_itr != fdata.first.begin() )
   {
      std::iter_swap( fork_itr, fdata.first.begin() );
   }

   return fdata;
}

rpc::chain::get_resource_limits_response controller_impl::get_resource_limits( const rpc::chain::get_resource_limits_request& )
{
   std::shared_lock< std::shared_mutex > lock( _db_mutex );

   execution_context ctx( _vm_backend );
   ctx.push_frame( stack_frame {
      .call_privilege = privilege::kernel_mode
   } );

   ctx.set_state_node( _db.get_head() );
   ctx.build_cache();

   auto value = system_call::get_resource_limits( ctx );

   rpc::chain::get_resource_limits_response resp;
   *resp.mutable_resource_limit_data() = value;

   return resp;
}

rpc::chain::get_account_rc_response controller_impl::get_account_rc( const rpc::chain::get_account_rc_request& request )
{
   std::shared_lock< std::shared_mutex > lock( _db_mutex );

   KOINOS_ASSERT( request.account().size(), missing_required_arguments, "missing expected field: ${f}", ("f", "payer") );

   execution_context ctx( _vm_backend );
   ctx.push_frame( stack_frame {
      .call_privilege = privilege::kernel_mode
   } );

   ctx.set_state_node( _db.get_head() );
   ctx.build_cache();

   auto value = system_call::get_account_rc( ctx, request.account() );

   rpc::chain::get_account_rc_response resp;
   resp.set_rc( value );

   return resp;
}

rpc::chain::get_fork_heads_response controller_impl::get_fork_heads( const rpc::chain::get_fork_heads_request& )
{
   rpc::chain::get_fork_heads_response resp;

   const auto [ fork_heads, last_irreversible_block ] = get_fork_data();
   auto topo = resp.mutable_last_irreversible_block();
   *topo = std::move( last_irreversible_block );

   for ( const auto& fork_head : fork_heads )
   {
      auto* head = resp.add_fork_heads();
      *head = fork_head;
   }

   return resp;
}

rpc::chain::read_contract_response controller_impl::read_contract( const rpc::chain::read_contract_request& request )
{
   std::shared_lock< std::shared_mutex > lock( _db_mutex );

   KOINOS_ASSERT( request.contract_id().size(), missing_required_arguments, "missing expected field: ${f}", ("f", "contract_id") );

   execution_context ctx( _vm_backend, intent::read_only );
   ctx.push_frame( stack_frame {
      .call_privilege = privilege::user_mode,
   } );

   ctx.set_state_node( _db.get_head() );
   ctx.build_cache();

   resource_limit_data rl;
   rl.set_compute_bandwidth_limit( _read_compute_bandwidth_limit );

   ctx.resource_meter().set_resource_limit_data( rl );

   auto result = system_call::call_contract( ctx, request.contract_id(), request.entry_point(), request.args() );

   rpc::chain::read_contract_response resp;
   resp.set_result( result );
   for ( const auto& message : ctx.chronicler().logs() )
      *resp.add_logs() = message;

   return resp;
}

rpc::chain::get_account_nonce_response controller_impl::get_account_nonce( const rpc::chain::get_account_nonce_request& request )
{
   std::shared_lock< std::shared_mutex > lock( _db_mutex );

   KOINOS_ASSERT( request.account().size(), missing_required_arguments, "missing expected field: ${f}", ("f", "account") );

   execution_context ctx( _vm_backend );

   ctx.push_frame( koinos::chain::stack_frame {
      .call_privilege = privilege::kernel_mode
   } );

   ctx.set_state_node( _db.get_head() );
   ctx.build_cache();

   auto nonce = system_call::get_account_nonce( ctx, request.account() );

   rpc::chain::get_account_nonce_response resp;
   resp.set_nonce( nonce );

   return resp;
}

} // detail

controller::controller( uint64_t read_compute_bandwith_limit ) : _my( std::make_unique< detail::controller_impl >( read_compute_bandwith_limit ) ) {}

controller::~controller() = default;

void controller::open( const std::filesystem::path& p, const chain::genesis_data& data, bool reset )
{
   _my->open( p, data, reset );
}

void controller::set_client( std::shared_ptr< mq::client > c )
{
   _my->set_client( c );
}

rpc::chain::submit_block_response controller::submit_block(
   const rpc::chain::submit_block_request& request,
   uint64_t index_to,
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

rpc::chain::get_account_rc_response controller::get_account_rc( const rpc::chain::get_account_rc_request& request )
{
   return _my->get_account_rc( request );
}

rpc::chain::get_resource_limits_response controller::get_resource_limits( const rpc::chain::get_resource_limits_request& request )
{
   return _my->get_resource_limits( request );
}

} // koinos::chain
