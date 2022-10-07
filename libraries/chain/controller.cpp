#include <koinos/block_store/block_store.pb.h>
#include <koinos/broadcast/broadcast.pb.h>

#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/constants.hpp>
#include <koinos/chain/controller.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/system_calls.hpp>

#include <koinos/exception.hpp>

#include <koinos/protocol/protocol.pb.h>

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

using fork_data = std::pair< std::vector< block_topology >, block_topology >;

namespace detail {

std::string format_time( int64_t time )
{
   std::stringstream ss;

   auto seconds = time % 60;
   time /= 60;
   auto minutes = time % 60;
   time /= 60;
   auto hours = time % 24;
   time /= 24;
   auto days = time % 365;
   auto years = time / 365;

   if ( years )
   {
      ss << years << "y, " << days << "d, ";
   }
   else if ( days )
   {
      ss << days << "d, ";
   }

   ss << std::setw(2) << std::setfill('0') << hours;
   ss << std::setw(1) << "h, ";
   ss << std::setw(2) << std::setfill('0') << minutes;
   ss << std::setw(1) << "m, ";
   ss << std::setw(2) << std::setfill('0') << seconds;
   ss << std::setw(1) << "s";
   return ss.str();
}

class controller_impl final
{
   public:
      controller_impl( uint64_t read_compute_bandwith_limit );
      ~controller_impl();

      void open( const std::filesystem::path& p, const genesis_data& data, fork_resolution_algorithm algo, bool reset );
      void close();
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
      std::shared_ptr< vm_manager::vm_backend > _vm_backend;
      std::shared_ptr< mq::client >             _client;
      uint64_t                                  _read_compute_bandwidth_limit;
      std::shared_mutex                         _cached_head_block_mutex;
      std::shared_ptr< const protocol::block >  _cached_head_block;

      void validate_block( const protocol::block& b );
      void validate_transaction( const protocol::transaction& t );

      fork_data get_fork_data( state_db::shared_lock_ptr db_lock );
};

controller_impl::controller_impl( uint64_t read_compute_bandwidth_limit ) : _read_compute_bandwidth_limit( read_compute_bandwidth_limit )
{
   _vm_backend = vm_manager::get_vm_backend(); // Default is fizzy
   KOINOS_ASSERT( _vm_backend, unknown_backend_exception, "could not get vm backend" );

   _cached_head_block = std::make_shared< const protocol::block >( protocol::block() );

   _vm_backend->initialize();
   LOG(info) << "Initialized " << _vm_backend->backend_name() << " VM backend";
}

controller_impl::~controller_impl()
{
   close();
}

void controller_impl::open( const std::filesystem::path& p, const chain::genesis_data& data, fork_resolution_algorithm algo, bool reset )
{
   state_db::state_node_comparator_function comp;

   switch( algo )
   {
      case fork_resolution_algorithm::block_time:
         comp = &state_db::block_time_comparator;
         break;
      case fork_resolution_algorithm::pob:
         comp = &state_db::pob_comparator;
         break;
      case fork_resolution_algorithm::fifo:
         [[fallthrough]];
      default:
         comp = &state_db::fifo_comparator;
   }

   _db.open( p, [&]( state_db::state_node_ptr root )
   {
      // Write genesis objects into the database
      for ( const auto& entry : data.entries() )
      {
         KOINOS_ASSERT(
            !root->get_object( entry.space(), entry.key() ),
            unexpected_state_exception,
            "encountered unexpected object in initial state"
         );

         root->put_object( entry.space(), entry.key(), &entry.value() );
      }
      LOG(info) << "Wrote " << data.entries().size() << " genesis objects into new database";

      // Read genesis public key from the database, assert its existence at the correct location
      KOINOS_ASSERT(
         root->get_object( state::space::metadata(), state::key::genesis_key ),
         unexpected_state_exception,
         "could not find genesis public key in database"
      );

      // Calculate and write the chain ID into the database
      auto chain_id = crypto::hash( koinos::crypto::multicodec::sha2_256, data );
      LOG(info) << "Calculated chain ID: " << chain_id;
      auto chain_id_str = util::converter::as< std::string >( chain_id );
      KOINOS_ASSERT(
         !root->get_object( chain::state::space::metadata(), chain::state::key::chain_id ),
         unexpected_state_exception,
         "encountered unexpected chain id in initial state"
      );

      root->put_object( chain::state::space::metadata(), chain::state::key::chain_id, &chain_id_str );
      LOG(info) << "Wrote chain ID into new database";
   }, comp, _db.get_unique_lock() );

   if ( reset )
   {
      LOG(info) << "Resetting database...";
      _db.reset( _db.get_unique_lock() );
   }

   auto head = _db.get_head( _db.get_shared_lock() );
   LOG(info) << "Opened database at block - Height: " << head->revision() << ", ID: " << head->id();
}

void controller_impl::close()
{
   _db.close( _db.get_unique_lock() );
}

void controller_impl::set_client( std::shared_ptr< mq::client > c )
{
   _client = c;
}

void controller_impl::validate_block( const protocol::block& b )
{
   KOINOS_ASSERT( b.id().size(), missing_required_arguments_exception, "missing expected field in block: ${field}", ("field", "id") );
   KOINOS_ASSERT( b.has_header(), missing_required_arguments_exception, "missing expected field in block: ${field}", ("field", "header")("block_id", util::to_hex( b.id() )) );
   KOINOS_ASSERT( b.header().previous().size(), missing_required_arguments_exception, "missing expected field in block header: ${field}", ("field", "previous")("block_id", util::to_hex( b.id() )) );
   KOINOS_ASSERT( b.header().height(), missing_required_arguments_exception, "missing expected field in block header: ${field}", ("field", "height")("block_id", util::to_hex( b.id() )) );
   KOINOS_ASSERT( b.header().timestamp(), missing_required_arguments_exception, "missing expected field in block header: ${field}", ("field", "timestamp")("block_id", util::to_hex( b.id() )) );
   KOINOS_ASSERT( b.header().previous_state_merkle_root().size(), missing_required_arguments_exception, "missing expected field in block header: ${field}", ("field", "previous_state_merkle_root")("block_id", util::to_hex( b.id() )) );
   KOINOS_ASSERT( b.header().transaction_merkle_root().size(), missing_required_arguments_exception, "missing expected field in block header: ${field}", ("field", "transaction_merkle_root")("block_id", util::to_hex( b.id() )) );
   KOINOS_ASSERT( b.signature().size(), missing_required_arguments_exception, "missing expected field in block: ${field}", ("field", "signature_data")("block_id", util::to_hex( b.id() )) );

   for ( const auto& t : b.transactions() )
      validate_transaction( t );
}

void controller_impl::validate_transaction( const protocol::transaction& t )
{
   KOINOS_ASSERT( t.id().size(), missing_required_arguments_exception, "missing expected field in transaction: ${field}", ("field", "id") );
   KOINOS_ASSERT( t.has_header(), missing_required_arguments_exception, "missing expected field in transaction: ${field}", ("field", "header")("transaction_id", util::to_hex( t.id() )) );
   KOINOS_ASSERT( t.header().rc_limit(), missing_required_arguments_exception, "missing expected field in transaction header: ${field}", ("field", "rc_limit")("transaction_id", util::to_hex( t.id() )) );
   KOINOS_ASSERT( t.header().operation_merkle_root().size(), missing_required_arguments_exception, "missing expected field in transaction header: ${field}", ("field", "operation_merkle_root")("transaction_id", util::to_hex( t.id() )) );
   KOINOS_ASSERT( t.signatures().size(), missing_required_arguments_exception, "missing expected field in transaction: ${field}", ("field", "signature_data")("transaction_id", util::to_hex( t.id() )) );
}

rpc::chain::submit_block_response controller_impl::submit_block(
   const rpc::chain::submit_block_request& request,
   uint64_t index_to,
   std::chrono::system_clock::time_point now )
{
   validate_block( request.block() );

   rpc::chain::submit_block_response resp;

   static constexpr uint64_t index_message_interval = 1000;
   static constexpr std::chrono::seconds time_delta = std::chrono::seconds( 5 );
   static constexpr std::chrono::seconds live_delta = std::chrono::seconds( 60 );

   auto time_lower_bound  = uint64_t( 0 );
   auto time_upper_bound  = std::chrono::duration_cast< std::chrono::milliseconds >( ( now + time_delta ).time_since_epoch() ).count();
   uint64_t parent_height = 0;

   auto db_lock = _db.get_shared_lock();

   const auto& block = request.block();
   auto block_id     = util::converter::to< crypto::multihash >( block.id() );
   auto block_height = block.header().height();
   auto parent_id    = util::converter::to< crypto::multihash >( block.header().previous() );
   auto block_node   = _db.get_node( block_id, db_lock );
   auto parent_node  = _db.get_node( parent_id, db_lock );

   bool new_head = false;

   if ( block_node ) return {}; // Block has been applied

   // This prevents returning "unknown previous block" when the pushed block is the LIB
   if ( !parent_node )
   {
      auto root = _db.get_root( db_lock );
      KOINOS_ASSERT( block_height >= root->revision(), pre_irreversibility_block_exception, "block is prior to irreversibility" );
      KOINOS_ASSERT( block_id == root->id(), unknown_previous_block_exception, "unknown previous block" );
      return {}; // Block is current LIB
   }

   bool live = block.header().timestamp() > std::chrono::duration_cast< std::chrono::milliseconds >( ( now - live_delta ).time_since_epoch() ).count();

   if ( !index_to && live )
   {
      LOG(info) << "Pushing block - Height: " << block_height << ", ID: " << block_id;
   }

   block_node = _db.create_writable_node( parent_id, block_id, block.header(), db_lock );

   // If this is not the genesis case, we must ensure that the proposed block timestamp is greater
   // than the parent block timestamp.
   if ( block_node && !parent_id.is_zero() )
   {
      execution_context parent_ctx( _vm_backend, intent::read_only );

      parent_ctx.push_frame( stack_frame {
         .call_privilege = privilege::kernel_mode
      } );

      parent_ctx.set_state_node( parent_node );
      parent_ctx.reset_cache();
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
         KOINOS_ASSERT( block_height == 1, unexpected_height_exception, "first block must have height of 1" );
      }

      KOINOS_ASSERT( block_node, block_state_error_exception, "could not create new block state node" );

      KOINOS_ASSERT(
         block_height == parent_height + 1,
         unexpected_height_exception,
         "expected block height of ${a}, was ${b}", ("a", parent_height + 1)("b", block_height)
      );

      KOINOS_ASSERT( block.header().timestamp() <= time_upper_bound, timestamp_out_of_bounds_exception, "block timestamp is too far in the future" );
      KOINOS_ASSERT( block.header().timestamp() >  time_lower_bound, timestamp_out_of_bounds_exception, "block timestamp is too old" );

      KOINOS_ASSERT(
         block.header().previous_state_merkle_root() == util::converter::as< std::string >( parent_node->merkle_root() ),
         state_merkle_mismatch_exception,
         "block previous state merkle mismatch"
      );

      ctx.push_frame( stack_frame {
         .call_privilege = privilege::kernel_mode
      } );

      ctx.set_state_node( block_node );
      ctx.reset_cache();

      system_call::apply_block( ctx, block );

      KOINOS_ASSERT( std::holds_alternative< protocol::block_receipt >( ctx.receipt() ), unexpected_receipt_exception, "expected block receipt" );
      *resp.mutable_receipt() = std::get< protocol::block_receipt >( ctx.receipt() );

      if ( _client )
      {
         rpc::block_store::block_store_request req;
         req.mutable_add_block()->mutable_block_to_add()->CopyFrom( block );
         req.mutable_add_block()->mutable_receipt_to_add()->CopyFrom( std::get< protocol::block_receipt >( ctx.receipt() ) );

         auto future = _client->rpc( util::service::block_store, util::converter::as< std::string >( req ), 1500ms, mq::retry_policy::none );

         rpc::block_store::block_store_response resp;
         resp.ParseFromString( future.get() );

         KOINOS_ASSERT( !resp.has_error(), rpc_failure_exception, "received error from block store: ${e}", ("e", resp.error()) );
         KOINOS_ASSERT( resp.has_add_block(), rpc_failure_exception, "unexpected response when submitting block: ${r}", ("r", resp) );
      }

      uint64_t disk_storage_used      = ctx.resource_meter().disk_storage_used();
      uint64_t network_bandwidth_used = ctx.resource_meter().network_bandwidth_used();
      uint64_t compute_bandwidth_used = ctx.resource_meter().compute_bandwidth_used();

      if ( !index_to && live )
      {
         auto num_transactions = block.transactions_size();

         LOG(info) << "Block application successful - Height: " << block_height << ", ID: " << block_id << " (" << num_transactions << ( num_transactions == 1 ? " transaction)" : " transactions)" );
         LOG(info) << "Consumed resources: " << disk_storage_used << " disk, " << network_bandwidth_used << " network, " << compute_bandwidth_used << " compute";
      }
      else if ( block_height % index_message_interval == 0 )
      {
         if ( index_to )
         {
            auto progress = block_height / static_cast< double >( index_to ) * 100;
            LOG(info) << "Indexing chain (" << progress << "%) - Height: " << block_height << ", ID: " << block_id;
         }
         else
         {
            auto to_go = std::chrono::duration_cast< std::chrono::seconds >( now.time_since_epoch() - std::chrono::milliseconds( block.header().timestamp() ) ).count();
            LOG(info) << "Sync progress - Height: " << block_height << ", ID: " << block_id << " (" << format_time( to_go ) << " block time remaining)";
         }
      }

      auto lib = system_call::get_last_irreversible_block( ctx );

      {
         // We need to finalize out node, checking if it is the new head block, and update the cached head block
         // as an atomic action or else we risk _db.get_head() and _cached_head_block desyncing from each other
         std::unique_lock< std::shared_mutex > head_lock( _cached_head_block_mutex );

         _db.finalize_node( block_node->id(), db_lock );

         if ( block_node->id() == _db.get_head( db_lock )->id() )
         {
            new_head = true;
            _cached_head_block = std::make_shared< protocol::block >( block );
         }
      }

      resp.mutable_receipt()->set_state_merkle_root( util::converter::as< std::string >( block_node->merkle_root() ) );

      if ( _client )
      {
         const auto [ fork_heads, last_irreversible_block ] = get_fork_data( db_lock );

         broadcast::block_irreversible bc;
         bc.mutable_topology()->CopyFrom( last_irreversible_block );

         _client->broadcast( "koinos.block.irreversible", util::converter::as< std::string >( bc ) );

         broadcast::block_accepted ba;
         *ba.mutable_block() = block;
         *ba.mutable_receipt() = std::get< protocol::block_receipt >( ctx.receipt() );
         ba.set_live( live );
         ba.set_head( new_head );

         _client->broadcast( "koinos.block.accept", util::converter::as< std::string >( ba ) );

         broadcast::fork_heads fh;
         fh.set_allocated_last_irreversible_block( bc.release_topology() );

         for ( const auto& fork_head : fork_heads )
         {
            auto* head = fh.add_heads();
            *head = fork_head;
         }

         _client->broadcast( "koinos.block.forks", util::converter::as< std::string >( fh ) );

         for ( const auto& [ transaction_id, event ] : ctx.chronicler().events() )
         {
            broadcast::event_parcel ep;
            ep.set_block_id( block.id() );
            ep.set_height( block.header().height() );
            *ep.mutable_event() = event;

            if ( transaction_id )
               ep.set_transaction_id( *transaction_id );

            _client->broadcast( "koinos.event." + util::to_base58( event.source() ) + "." + event.name(), ep.SerializeAsString() );
         }
      }

      if ( lib > _db.get_root( db_lock )->revision() )
      {
         auto lib_id = _db.get_node_at_revision( lib, block_node->id(), db_lock )->id();
         db_lock.reset();
         block_node.reset();
         parent_node.reset();
         ctx.clear_state_node();

         _db.commit_node( lib_id, _db.get_unique_lock() );

         db_lock = _db.get_shared_lock();
      }
   }
   catch ( koinos::exception& e )
   {
      if ( block_node && !block_node->is_finalized() )
      {
         _db.discard_node( block_node->id(), db_lock );
         LOG(warning) << "Block application failed - Height: " << block_height << " ID: " << block_id << ", with reason: " << e.what();
      }
      else
      {
         LOG(error) << "Block application failed after finalization - Height: " << block_height << " ID: " << block_id << ", with reason: " << e.what();
      }

      if ( _client )
      {
         const auto& exception_data = e.get_json();

         if ( exception_data.count( "transaction_id" ) )
         {
            broadcast::transaction_failed ptf;
            ptf.set_id( util::from_hex< std::string >( exception_data[ "transaction_id" ] ) );
            _client->broadcast( "koinos.transaction.fail", util::converter::as< std::string >( ptf ) );
         }
      }

      if ( std::holds_alternative< protocol::block_receipt >( ctx.receipt() ) )
         e.add_json( "logs", std::get< protocol::block_receipt >( ctx.receipt() ).logs() );

      throw;
   }
   catch ( ... )
   {
      if ( block_node && !block_node->is_finalized() )
      {
         _db.discard_node( block_node->id(), db_lock );
         LOG(warning) << "Block application failed - Height: " << block_height << ", ID: " << block_id << ", for an unknown reason";
      }
      else
      {
         LOG(error) << "Block application failed after finalization - Height: " << block_height << ", ID: " << block_id << ", for an unknown reason";
      }

      throw;
   }

   return resp;
}

rpc::chain::submit_transaction_response controller_impl::submit_transaction( const rpc::chain::submit_transaction_request& request )
{
   validate_transaction( request.transaction() );

   rpc::chain::submit_transaction_response resp;

   std::string payer;
   uint64_t max_payer_rc;
   uint64_t trx_rc_limit;

   auto transaction    = request.transaction();
   auto transaction_id = util::to_hex( transaction.id() );

   LOG(info) << "Pushing transaction - ID: " << transaction_id;

   auto db_lock = _db.get_shared_lock();
   state_node_ptr head;
   execution_context ctx( _vm_backend, intent::transaction_application );
   std::shared_ptr< const protocol::block > head_block_ptr;

   {
      std::shared_lock< std::shared_mutex > head_lock( _cached_head_block_mutex );
      head_block_ptr = _cached_head_block;
      KOINOS_ASSERT( head_block_ptr, internal_error_exception, "error retrieving head block" );

      head = _db.get_head( db_lock );
   }

   ctx.set_block( *head_block_ptr );
   ctx.set_state_node( head->create_anonymous_node() );

   ctx.push_frame( stack_frame {
      .call_privilege = privilege::kernel_mode
   } );

   try
   {
      ctx.reset_cache();

      payer = transaction.header().payer();

      max_payer_rc = system_call::get_account_rc( ctx, payer );

      trx_rc_limit = transaction.header().rc_limit();

      ctx.resource_meter().set_resource_limit_data( system_call::get_resource_limits( ctx ) );

      system_call::apply_transaction( ctx, transaction );

      uint64_t disk_storage_used      = ctx.resource_meter().disk_storage_used();
      uint64_t network_bandwidth_used = ctx.resource_meter().network_bandwidth_used();
      uint64_t compute_bandwidth_used = ctx.resource_meter().compute_bandwidth_used();

      if ( _client )
      {
         rpc::mempool::mempool_request req;
         auto* check_pending = req.mutable_check_pending_account_resources();

         check_pending->set_payer( payer );
         check_pending->set_max_payer_rc( max_payer_rc );
         check_pending->set_rc_limit( trx_rc_limit );

         auto future = _client->rpc( util::service::mempool, util::converter::as< std::string >( req ), 750ms, mq::retry_policy::none );

         rpc::mempool::mempool_response resp;
         resp.ParseFromString( future.get() );

         KOINOS_ASSERT( !resp.has_error(), rpc_failure_exception, "received error from mempool: ${e}", ("e", resp.error()) );
         KOINOS_ASSERT( resp.has_check_pending_account_resources(), rpc_failure_exception, "received unexpected response from mempool" );
      }

      LOG(info) << "Transaction application successful - ID: " << transaction_id;

      KOINOS_ASSERT( std::holds_alternative< protocol::transaction_receipt >( ctx.receipt() ), unexpected_receipt_exception, "expected transaction receipt" );
      *resp.mutable_receipt() = std::get< protocol::transaction_receipt >( ctx.receipt() );

      if ( request.broadcast() && _client )
      {
         broadcast::transaction_accepted ta;
         *ta.mutable_transaction() = transaction;
         *ta.mutable_receipt() = std::get< protocol::transaction_receipt >( ctx.receipt() );
         ta.set_height( ctx.get_state_node()->revision() );

         _client->broadcast( "koinos.transaction.accept", util::converter::as< std::string >( ta ) );
      }
   }
   catch ( koinos::exception& e )
   {
      LOG(warning) << "Transaction application failed - ID: " << transaction_id << ", with reason: " << e.what();

      if ( std::holds_alternative< protocol::transaction_receipt >( ctx.receipt() ) )
         e.add_json( "logs", std::get< protocol::transaction_receipt >( ctx.receipt() ).logs() );

      throw;
   }
   catch ( ... )
   {
      LOG(warning) << "Transaction application failed - ID: " << transaction_id << ", for an unknown reason";
      throw;
   }

   return resp;
}

rpc::chain::get_head_info_response controller_impl::get_head_info( const rpc::chain::get_head_info_request& )
{
   execution_context ctx( _vm_backend );
   ctx.push_frame( stack_frame {
      .call_privilege = privilege::kernel_mode
   } );

   auto db_lock = _db.get_shared_lock();
   state_node_ptr head;
   std::shared_ptr< const protocol::block > head_block_ptr;

   {
      std::shared_lock< std::shared_mutex > head_lock( _cached_head_block_mutex );
      head_block_ptr = _cached_head_block;

      KOINOS_ASSERT( head_block_ptr, internal_error_exception, "error retrieving head block" );

      head = _db.get_head( db_lock );
   }

   ctx.set_state_node( head->create_anonymous_node() );

   KOINOS_ASSERT( head_block_ptr, internal_error_exception, "error retrieving head block" );

   ctx.set_block( *head_block_ptr );
   ctx.reset_cache();

   auto head_info = system_call::get_head_info( ctx );
   block_topology topo = head_info.head_topology();

   rpc::chain::get_head_info_response resp;
   *resp.mutable_head_topology() = topo;
   resp.set_last_irreversible_block( head_info.last_irreversible_block() );
   resp.set_head_state_merkle_root( util::converter::as< std::string >( head->merkle_root() ) );
   resp.set_head_block_time( head_info.head_block_time() );

   return resp;
}

rpc::chain::get_chain_id_response controller_impl::get_chain_id( const rpc::chain::get_chain_id_request& )
{
   execution_context ctx( _vm_backend );
   ctx.push_frame( stack_frame {
      .call_privilege = privilege::kernel_mode
   } );

   ctx.set_state_node( _db.get_head( _db.get_shared_lock() )->create_anonymous_node() );
   ctx.reset_cache();

   rpc::chain::get_chain_id_response resp;
   resp.set_chain_id( system_call::get_chain_id( ctx ) );

   return resp;
}

fork_data controller_impl::get_fork_data( state_db::shared_lock_ptr db_lock )
{
   fork_data fdata;
   execution_context ctx( _vm_backend );

   ctx.push_frame( koinos::chain::stack_frame {
      .call_privilege = privilege::kernel_mode
   } );

   std::vector< state_db::state_node_ptr > fork_heads;

   ctx.set_state_node( _db.get_root( db_lock )->create_anonymous_node() );
   ctx.reset_cache();
   fork_heads = _db.get_fork_heads( db_lock );

   auto head_info = system_call::get_head_info( ctx );
   fdata.second = head_info.head_topology();

   for ( auto& fork : fork_heads )
   {
      ctx.set_state_node( fork->create_anonymous_node() );
      ctx.reset_cache();
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
   execution_context ctx( _vm_backend );
   ctx.push_frame( stack_frame {
      .call_privilege = privilege::kernel_mode
   } );

   ctx.set_state_node( _db.get_head( _db.get_shared_lock() )->create_anonymous_node() );
   ctx.reset_cache();

   auto value = system_call::get_resource_limits( ctx );

   rpc::chain::get_resource_limits_response resp;
   *resp.mutable_resource_limit_data() = value;

   return resp;
}

rpc::chain::get_account_rc_response controller_impl::get_account_rc( const rpc::chain::get_account_rc_request& request )
{
   KOINOS_ASSERT( request.account().size(), missing_required_arguments_exception, "missing expected field: ${f}", ("f", "payer") );

   execution_context ctx( _vm_backend );
   ctx.push_frame( stack_frame {
      .call_privilege = privilege::kernel_mode
   } );

   ctx.set_state_node( _db.get_head( _db.get_shared_lock() )->create_anonymous_node() );
   ctx.reset_cache();

   auto value = system_call::get_account_rc( ctx, request.account() );

   rpc::chain::get_account_rc_response resp;
   resp.set_rc( value );

   return resp;
}

rpc::chain::get_fork_heads_response controller_impl::get_fork_heads( const rpc::chain::get_fork_heads_request& )
{
   rpc::chain::get_fork_heads_response resp;

   const auto [ fork_heads, last_irreversible_block ] = get_fork_data( _db.get_shared_lock() );
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
   KOINOS_ASSERT( request.contract_id().size(), missing_required_arguments_exception, "missing expected field: ${f}", ("f", "contract_id") );

   auto db_lock = _db.get_shared_lock();

   execution_context ctx( _vm_backend, intent::read_only );
   ctx.push_frame( stack_frame {
      .call_privilege = privilege::user_mode,
   } );

   std::shared_ptr< const protocol::block > head_block_ptr;

   {
      std::shared_lock< std::shared_mutex > head_lock( _cached_head_block_mutex );
      head_block_ptr = _cached_head_block;
      KOINOS_ASSERT( head_block_ptr, internal_error_exception, "error retrieving head block" );

      ctx.set_state_node( _db.get_head( db_lock )->create_anonymous_node() );
   }

   ctx.set_block( *head_block_ptr );
   ctx.reset_cache();

   resource_limit_data rl;
   rl.set_compute_bandwidth_limit( _read_compute_bandwidth_limit );

   ctx.resource_meter().set_resource_limit_data( rl );

   rpc::chain::read_contract_response resp;
   resp.set_result( system_call::call( ctx, request.contract_id(), request.entry_point(), request.args() ) );

   for ( const auto& message : ctx.chronicler().logs() )
      *resp.add_logs() = message;

   return resp;
}

rpc::chain::get_account_nonce_response controller_impl::get_account_nonce( const rpc::chain::get_account_nonce_request& request )
{
   KOINOS_ASSERT( request.account().size(), missing_required_arguments_exception, "missing expected field: ${f}", ("f", "account") );

   execution_context ctx( _vm_backend );

   ctx.push_frame( koinos::chain::stack_frame {
      .call_privilege = privilege::kernel_mode
   } );

   ctx.set_state_node( _db.get_head( _db.get_shared_lock() )->create_anonymous_node() );
   ctx.reset_cache();

   auto nonce = system_call::get_account_nonce( ctx, request.account() );

   rpc::chain::get_account_nonce_response resp;
   resp.set_nonce( nonce );

   return resp;
}

} // detail

controller::controller( uint64_t read_compute_bandwith_limit ) : _my( std::make_unique< detail::controller_impl >( read_compute_bandwith_limit ) ) {}

controller::~controller() = default;

void controller::open( const std::filesystem::path& p, const chain::genesis_data& data, fork_resolution_algorithm algo, bool reset )
{
   _my->open( p, data, algo, reset );
}

void controller::close()
{
   _my->close();
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
