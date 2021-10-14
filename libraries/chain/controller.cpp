#include <koinos/block_store/block_store.pb.h>
#include <koinos/broadcast/broadcast.pb.h>

#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/constants.hpp>
#include <koinos/chain/controller.hpp>

#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/system_calls.hpp>

#include <koinos/conversion.hpp>

#include <koinos/rpc/block_store/block_store_rpc.pb.h>
#include <koinos/rpc/chain/chain_rpc.pb.h>
#include <koinos/rpc/mempool/mempool_rpc.pb.h>

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

#include <boost/interprocess/streams/vectorstream.hpp>

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

      fork_data get_fork_data();
      fork_data get_fork_data_lockless();
};

controller_impl::controller_impl()
{
   _vm_backend = vm_manager::get_vm_backend(); // Default is fizzy
   KOINOS_ASSERT( _vm_backend, unknown_backend_exception, "could not get vm backend" );

   _vm_backend->initialize();

   LOG(info) << "Initialized " << _vm_backend->backend_name() << " vm backend";
}

controller_impl::~controller_impl()
{
   std::lock_guard< std::shared_mutex > lock( _db_mutex );
   _db.close();
}

void controller_impl::open( const std::filesystem::path& p, const std::any& o, const genesis_data& data, bool reset )
{
   std::lock_guard< std::shared_mutex > lock( _db_mutex );

   _db.open( p, o, [&]( state_db::state_node_ptr root )
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
      _db.reset();
   }

   auto head = _db.get_head();
   LOG(info) << "Opened database at block - Height: " << head->revision() << ", ID: " << head->id();
}

void controller_impl::set_client( std::shared_ptr< mq::client > c )
{
   _client = c;
}

rpc::chain::submit_block_response controller_impl::submit_block(
   const rpc::chain::submit_block_request& request,
   uint64_t index_to,
   std::chrono::system_clock::time_point now )
{
   std::lock_guard< std::shared_mutex > lock( _db_mutex );

   KOINOS_ASSERT( request.block().id().size(), missing_required_arguments, "missing expected field in block: ${f}", ("f", "id") );
   KOINOS_ASSERT( request.block().has_header(), missing_required_arguments, "missing expected field in block: ${f}", ("f", "header") );
   KOINOS_ASSERT( request.block().header().previous().size(), missing_required_arguments, "missing expected field in block_header: ${f}", ("f", "previous") );
   KOINOS_ASSERT( request.block().header().height(), missing_required_arguments, "missing expected field in block_header: ${f}", ("f", "height") );
   KOINOS_ASSERT( request.block().header().timestamp(), missing_required_arguments, "missing expected field in block_header: ${f}", ("f", "timestamp") );
   KOINOS_ASSERT( request.block().active().size(), missing_required_arguments, "missing expected field in block: ${f}", ("f", "active") );
   KOINOS_ASSERT( request.block().passive().size() == 0, missing_required_arguments, "unexpected value in field in block: ${f}", ("f", "passive") );
   KOINOS_ASSERT( request.block().signature_data().size(), missing_required_arguments, "missing expected field in block: ${f}", ("f", "signature_data") );

   static constexpr uint64_t index_message_interval = 1000;
   static constexpr std::chrono::seconds time_delta = std::chrono::seconds( 5 );

   auto time_lower_bound  = uint64_t( 0 );
   auto time_upper_bound  = std::chrono::duration_cast< std::chrono::milliseconds >( ( now + time_delta ).time_since_epoch() ).count();
   uint64_t parent_height = 0;

   state_db::state_node_ptr block_node;

   auto block        = request.block();
   auto block_id     = converter::to< crypto::multihash >( block.id() );
   auto block_height = block.header().height();
   auto parent_id    = converter::to< crypto::multihash >( block.header().previous() );
   block_node        = _db.get_node( block_id );

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
      apply_context parent_ctx( _vm_backend );

      parent_ctx.push_frame( stack_frame {
         .call = crypto::hash( crypto::multicodec::ripemd_160, "submit_block"s ).digest(),
         .call_privilege = privilege::kernel_mode
      } );

      auto parent_node = _db.get_node( parent_id );
      parent_ctx.set_state_node( parent_node );
      auto head_info = system_call::get_head_info( parent_ctx ).value();
      parent_height = head_info.head_topology().height();
      time_lower_bound = head_info.head_block_time();
   }

   apply_context ctx( _vm_backend );

   try
   {
      // Genesis case, when the first block is submitted the previous must be the zero hash
      if ( parent_id.is_zero() )
      {
         KOINOS_ASSERT( block_height == 1, unexpected_height, "first block must have height of 1" );
      }

      KOINOS_ASSERT(
         crypto::hash( crypto::multicodec::sha2_256, block.header(), block.active() ) == block_id,
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

      ctx.push_frame( stack_frame {
         .call = crypto::hash( crypto::multicodec::ripemd_160, "submit_block"s ).digest(),
         .call_privilege = privilege::kernel_mode
      } );

      ctx.set_state_node( block_node );

      system_call::apply_block(
         ctx,
         block,
         request.verify_passive_data(),
         request.verify_block_signature(),
         request.verify_transaction_signature() );

      if ( _client && _client->is_connected() )
      {
         rpc::block_store::block_store_request req;
         req.mutable_add_block()->mutable_block_to_add()->CopyFrom( block );

         auto future = _client->rpc( service::block_store, converter::as< std::string >( req ), 750 /* ms */, mq::retry_policy::none );

         rpc::block_store::block_store_response resp;
         resp.ParseFromString( future.get() );

         KOINOS_ASSERT( !resp.has_error(), koinos::exception, "received error from block store: ${e}", ("e", resp.error()) );
         KOINOS_ASSERT( resp.has_add_block(), koinos::exception, "unexpected response when submitting block: ${r}", ("r", resp) );
      }

      if ( !index_to )
      {
         auto num_transactions = block.transactions_size();

         LOG(info) << "Block application successful - Height: " << block_height << ", ID: " << block_id << " (" << num_transactions << ( num_transactions == 1 ? " transaction)" : " transactions)" );

         auto disk = ctx.resource_meter().disk_storage_used();
         auto network = ctx.resource_meter().network_bandwidth_used();
         auto compute = ctx.resource_meter().compute_bandwidth_used();
         LOG(info) << "Consumed resources: " << disk << " disk, " << network << " network, " << compute << " compute";
      }
      else if ( block_height % index_message_interval == 0 )
      {
         auto progress = block_height / static_cast< double >( index_to ) * 100;
         LOG(info) << "Indexing chain (" << progress << "%) - Height: " << block_height << ", ID: " << block_id;
      }

      auto output = ctx.get_pending_console_output();

      if ( output.length() > 0 )
      {
         LOG(info) << "Output:\n" << output;
      }

      auto lib = system_call::get_last_irreversible_block( ctx ).value();

      _db.finalize_node( block_node->id() );

      if ( std::optional< state_node_ptr > node; lib > _db.get_root()->revision() )
      {
         node = _db.get_node_at_revision( lib, block_node->id() );
         _db.commit_node( node.value()->id() );
      }

      const auto [ fork_heads, last_irreversible_block ] = get_fork_data_lockless();

      if ( _client && _client->is_connected() )
      {
         try
         {
            broadcast::block_irreversible bc;
            bc.mutable_topology()->CopyFrom( last_irreversible_block );

            _client->broadcast( "koinos.block.irreversible", converter::as< std::string >( bc ) );
         }
         catch ( const std::exception& e )
         {
            LOG(error) << "Failed to publish block irreversible to message broker: " << e.what();
         }

         try
         {
            broadcast::block_accepted ba;

            ba.mutable_block()->CopyFrom( block );

            _client->broadcast( "koinos.block.accept", converter::as< std::string >( ba ) );
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

            _client->broadcast( "koinos.block.forks", converter::as< std::string >( fh ) );
         }
         catch ( const std::exception& e )
         {
            LOG(error) << "Failed to publish fork data to message broker: " << e.what();
         }
      }
   }
   catch( const std::exception& e )
   {
      LOG(warning) << "Block application failed - Height: " << block_height << " ID: " << block_id << ", with reason: " << e.what();

      auto output = ctx.get_pending_console_output();

      if ( output.length() > 0 )
      {
         LOG(info) << "Output:\n" << output;
      }

      if ( block_node )
      {
         _db.discard_node( block_node->id() );
      }

      throw;
   }
   catch( ... )
   {
      LOG(warning) << "Block application failed - Height: " << block_height << ", ID: " << block_id << ", for an unknown reason";

      auto output = ctx.get_pending_console_output();

      if ( output.length() > 0 )
      {
         LOG(info) << "Output:\n" << output;
      }

      if ( block_node )
      {
         _db.discard_node( block_node->id() );
      }

      throw;
   }

   return {};
}

rpc::chain::submit_transaction_response controller_impl::submit_transaction( const rpc::chain::submit_transaction_request& request )
{
   std::shared_lock< std::shared_mutex > lock( _db_mutex );

   KOINOS_ASSERT( request.transaction().id().size(), missing_required_arguments, "missing expected field in transaction: ${f}", ("f", "id") );
   KOINOS_ASSERT( request.transaction().active().size(), missing_required_arguments, "missing expected field in transaction: ${f}", ("f", "active") );
   KOINOS_ASSERT( request.transaction().passive().size() == 0, missing_required_arguments, "unexpected value in field in transaction: ${f}", ("f", "passive") );
   KOINOS_ASSERT( request.transaction().signature_data().size(), missing_required_arguments, "missing expected field in transaction: ${f}", ("f", "signature_data") );

   std::string payer;
   uint64_t max_payer_rc;
   uint64_t trx_rc_limit;

   auto transaction    = request.transaction();
   auto transaction_id = to_hex( transaction.id() );

   LOG(info) << "Pushing transaction - ID: " << transaction_id;

   auto pending_trx_node = _db.get_head()->create_anonymous_node();
   KOINOS_ASSERT( pending_trx_node, trx_state_error, "error creating pending transaction state node" );

   apply_context ctx( _vm_backend );

   ctx.push_frame( stack_frame {
      .call = crypto::hash( crypto::multicodec::sha2_256, "submit_transaction"s ).digest(),
      .call_privilege = privilege::kernel_mode
   } );

   try
   {
      ctx.set_state_node( pending_trx_node );

      payer = system_call::get_transaction_payer( ctx, transaction ).value();
      max_payer_rc = system_call::get_account_rc( ctx, payer ).value();
      trx_rc_limit = system_call::get_transaction_rc_limit( ctx, transaction ).value();

      ctx.resource_meter().set_resource_limit_data( system_call::get_resource_limits( ctx ).value() );

      system_call::apply_transaction( ctx, transaction );

      uint64_t disk_storage_used = ctx.resource_meter().disk_storage_used();
      uint64_t network_bandwidth_used = ctx.resource_meter().network_bandwidth_used();
      uint64_t compute_bandwidth_used = ctx.resource_meter().compute_bandwidth_used();

      if ( _client && _client->is_connected() )
      {
         rpc::mempool::mempool_request req;
         auto* check_pending = req.mutable_check_pending_account_resources();

         check_pending->set_payer( payer );
         check_pending->set_max_payer_rc( max_payer_rc );
         check_pending->set_rc_limit( trx_rc_limit );

         auto future = _client->rpc( service::mempool, converter::as< std::string >( req ), 750 /* ms */, mq::retry_policy::none );

         rpc::mempool::mempool_response resp;
         resp.ParseFromString( future.get() );

         KOINOS_ASSERT( !resp.has_error(), koinos::exception, "received error from mempool: ${e}", ("e", resp.error()) );
         KOINOS_ASSERT( resp.has_check_pending_account_resources(), koinos::exception, "received unexpected response from mempool" );
      }

      LOG(info) << "Transaction application successful - ID: " << transaction_id;

      if ( _client && _client->is_connected() )
      {
         try
         {
            broadcast::transaction_accepted ta;

            *ta.mutable_transaction() = transaction;
            ta.set_payer( payer );
            ta.set_max_payer_rc( max_payer_rc );
            ta.set_rc_limit( trx_rc_limit );
            ta.set_height( ctx.get_state_node()->revision() );
            ta.set_disk_storage_used( disk_storage_used );
            ta.set_network_bandwidth_used( network_bandwidth_used );
            ta.set_compute_bandwidth_used( compute_bandwidth_used );

            _client->broadcast( "koinos.transaction.accept", converter::as< std::string >( ta ) );
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
      auto output = ctx.get_pending_console_output();

      if ( output.length() > 0 )
      {
         LOG(info) << "Output:\n" << output;
      }

      throw;
   }
   catch( ... )
   {
      LOG(warning) << "Transaction application failed - ID: " << transaction_id << ", for an unknown reason";

      auto output = ctx.get_pending_console_output();

      if ( output.length() > 0 )
      {
         LOG(info) << "Output:\n" << output;
      }

      throw;
   }

   return {};
}

rpc::chain::get_head_info_response controller_impl::get_head_info( const rpc::chain::get_head_info_request& )
{
   std::shared_lock< std::shared_mutex > lock( _db_mutex );

   apply_context ctx( _vm_backend );
   ctx.push_frame( stack_frame {
      .call = crypto::hash( crypto::multicodec::ripemd_160, "get_head_info"s ).digest(),
      .call_privilege = privilege::kernel_mode
   } );

   ctx.set_state_node( _db.get_head() );

   auto head_info = system_call::get_head_info( ctx ).value();
   block_topology topo = head_info.head_topology();

   rpc::chain::get_head_info_response resp;
   *resp.mutable_head_topology() = topo;
   resp.set_last_irreversible_block( head_info.last_irreversible_block() );

   return resp;
}

rpc::chain::get_chain_id_response controller_impl::get_chain_id( const rpc::chain::get_chain_id_request& )
{
   std::shared_lock< std::shared_mutex > lock( _db_mutex );

   boost::interprocess::basic_ivectorstream< std::vector< char > > chain_id_stream;
   std::vector< char > chain_id_vector;
   chain_id_vector.resize( 128 );
   chain_id_stream.swap_vector( chain_id_vector );

   state_db::get_object_result result;
   state_db::get_object_args   args;
   args.space    = converter::as< state_db::object_space >( database::space::kernel );
   args.key      = converter::as< state_db::object_key >( database::key::chain_id );
   args.buf      = const_cast< std::byte* >( reinterpret_cast< const std::byte* >( chain_id_stream.vector().data() ) );
   args.buf_size = chain_id_stream.vector().size();

   state_db::state_node_ptr head;

   head = _db.get_head();

   head->get_object( result, args );

   KOINOS_ASSERT( result.key == args.key, retrieval_failure, "unable to retrieve chain id" );
   KOINOS_ASSERT( result.size <= args.buf_size, insufficent_buffer_size, "chain id buffer overflow" );

   crypto::multihash chain_id;
   from_binary( chain_id_stream, chain_id );

   LOG(debug) << "get_chain_id: " << chain_id;

   rpc::chain::get_chain_id_response resp;
   resp.set_chain_id( converter::as< std::string >( chain_id ) );

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
   apply_context ctx( _vm_backend );

   ctx.push_frame( koinos::chain::stack_frame {
      .call = crypto::hash( crypto::multicodec::ripemd_160, "get_fork_data"s ).digest(),
      .call_privilege = privilege::kernel_mode
   } );

   std::vector< state_db::state_node_ptr > fork_heads;

   ctx.set_state_node( _db.get_root() );
   fork_heads = _db.get_fork_heads();

   auto head_info = system_call::get_head_info( ctx ).value();
   fdata.second = head_info.head_topology();

   for ( auto& fork : fork_heads )
   {
      ctx.set_state_node( fork );
      auto head_info = system_call::get_head_info( ctx ).value();
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

   apply_context ctx( _vm_backend );
   ctx.push_frame( stack_frame {
      .call = crypto::hash( crypto::multicodec::ripemd_160, "get_resource_limits"s ).digest(),
      .call_privilege = privilege::kernel_mode
   } );

   ctx.set_state_node( _db.get_head() );

   auto value = system_call::get_resource_limits( ctx ).value();

   rpc::chain::get_resource_limits_response resp;
   *resp.mutable_resource_limit_data() = value;

   return resp;
}

rpc::chain::get_account_rc_response controller_impl::get_account_rc( const rpc::chain::get_account_rc_request& request )
{
   std::shared_lock< std::shared_mutex > lock( _db_mutex );

   KOINOS_ASSERT( request.account().size(), missing_required_arguments, "missing expected field: ${f}", ("f", "payer") );

   apply_context ctx( _vm_backend );
   ctx.push_frame( stack_frame {
      .call = crypto::hash( crypto::multicodec::ripemd_160, "get_account_rc"s ).digest(),
      .call_privilege = privilege::kernel_mode
   } );

   ctx.set_state_node( _db.get_head() );

   auto value = system_call::get_account_rc( ctx, request.account() ).value();

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

   state_db::state_node_ptr head_node;

   head_node = _db.get_head();

   apply_context ctx( _vm_backend );
   ctx.push_frame( stack_frame {
      .call = crypto::hash( crypto::multicodec::ripemd_160, "read_contract"s ).digest(),
      .call_privilege = privilege::kernel_mode,
   } );

   ctx.set_state_node( head_node );
   ctx.set_read_only( true );

   resource_limit_data rl;
   rl.set_compute_bandwidth_limit( 10'000'000 );

   ctx.resource_meter().set_resource_limit_data( rl );

   auto result = system_call::call_contract( ctx, request.contract_id(), request.entry_point(), request.args() ).value();

   rpc::chain::read_contract_response resp;
   resp.set_result( result );
   resp.set_logs( ctx.get_pending_console_output() );

   return resp;
}

rpc::chain::get_account_nonce_response controller_impl::get_account_nonce( const rpc::chain::get_account_nonce_request& request )
{
   std::shared_lock< std::shared_mutex > lock( _db_mutex );

   KOINOS_ASSERT( request.account().size(), missing_required_arguments, "missing expected field: ${f}", ("f", "account") );

   apply_context ctx( _vm_backend );

   ctx.push_frame( koinos::chain::stack_frame {
      .call = crypto::hash( crypto::multicodec::ripemd_160, "get_account_nonce"s ).digest(),
      .call_privilege = privilege::kernel_mode
   } );

   ctx.set_state_node( _db.get_head() );

   auto nonce = system_call::get_account_nonce( ctx, request.account() ).value();

   rpc::chain::get_account_nonce_response resp;
   resp.set_nonce( nonce );

   return resp;
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
