#include <koinos/block_store/block_store.pb.h>
#include <koinos/broadcast/broadcast.pb.h>

#include <koinos/chain/constants.hpp>
#include <koinos/chain/controller.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/host_api.hpp>
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

  auto seconds  = time % 60;
  time         /= 60;
  auto minutes  = time % 60;
  time         /= 60;
  auto hours    = time % 24;
  time         /= 24;
  auto days     = time % 365;
  auto years    = time / 365;

  if( years )
  {
    ss << years << "y, " << days << "d, ";
  }
  else if( days )
  {
    ss << days << "d, ";
  }

  ss << std::setw( 2 ) << std::setfill( '0' ) << hours;
  ss << std::setw( 1 ) << "h, ";
  ss << std::setw( 2 ) << std::setfill( '0' ) << minutes;
  ss << std::setw( 1 ) << "m, ";
  ss << std::setw( 2 ) << std::setfill( '0' ) << seconds;
  ss << std::setw( 1 ) << "s";
  return ss.str();
}

struct apply_block_options
{
  uint64_t index_to;
  std::chrono::system_clock::time_point application_time;
  bool propose_block;
};

struct apply_block_result
{
  std::optional< protocol::block_receipt > receipt;
  std::vector< uint32_t > failed_transaction_indices;
};

class controller_impl final
{
public:
  controller_impl( uint64_t read_compute_bandwith_limit, uint32_t syscall_bufsize );
  ~controller_impl();

  void open( const std::filesystem::path& p, const genesis_data& data, fork_resolution_algorithm algo, bool reset );
  void close();
  void set_client( std::shared_ptr< mq::client > c );

  apply_block_result apply_block( const protocol::block& block, const apply_block_options& opts );

  rpc::chain::submit_transaction_response submit_transaction( const rpc::chain::submit_transaction_request& );
  rpc::chain::get_head_info_response get_head_info( const rpc::chain::get_head_info_request& );
  rpc::chain::get_chain_id_response get_chain_id( const rpc::chain::get_chain_id_request& );
  rpc::chain::get_fork_heads_response get_fork_heads( const rpc::chain::get_fork_heads_request& );
  rpc::chain::read_contract_response read_contract( const rpc::chain::read_contract_request& );
  rpc::chain::get_account_nonce_response get_account_nonce( const rpc::chain::get_account_nonce_request& );
  rpc::chain::get_account_rc_response get_account_rc( const rpc::chain::get_account_rc_request& );
  rpc::chain::get_resource_limits_response get_resource_limits( const rpc::chain::get_resource_limits_request& );
  rpc::chain::invoke_system_call_response invoke_system_call( const rpc::chain::invoke_system_call_request& );

private:
  state_db::database _db;
  std::shared_ptr< vm_manager::vm_backend > _vm_backend;
  std::shared_ptr< mq::client > _client;
  uint64_t _read_compute_bandwidth_limit;
  uint32_t _syscall_bufsize;
  std::shared_mutex _cached_head_block_mutex;
  std::shared_ptr< const protocol::block > _cached_head_block;

  void validate_block( const protocol::block& b );
  void validate_transaction( const protocol::transaction& t );

  fork_data get_fork_data( state_db::shared_lock_ptr db_lock );
};

controller_impl::controller_impl( uint64_t read_compute_bandwidth_limit, uint32_t syscall_bufsize ):
    _read_compute_bandwidth_limit( read_compute_bandwidth_limit ),
    _syscall_bufsize( syscall_bufsize )
{
  _vm_backend = vm_manager::get_vm_backend(); // Default is fizzy
  KOINOS_ASSERT( _vm_backend, unknown_backend_exception, "could not get vm backend" );

  _cached_head_block = std::make_shared< const protocol::block >( protocol::block() );

  _vm_backend->initialize();
  LOG( info ) << "Initialized " << _vm_backend->backend_name() << " VM backend";
}

controller_impl::~controller_impl()
{
  close();
}

void controller_impl::open( const std::filesystem::path& p,
                            const chain::genesis_data& data,
                            fork_resolution_algorithm algo,
                            bool reset )
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

  _db.open(
    p,
    [ & ]( state_db::state_node_ptr root )
    {
      // Write genesis objects into the database
      for( const auto& entry: data.entries() )
      {
        KOINOS_ASSERT( !root->get_object( entry.space(), entry.key() ),
                       unexpected_state_exception,
                       "encountered unexpected object in initial state" );

        root->put_object( entry.space(), entry.key(), &entry.value() );
      }
      LOG( info ) << "Wrote " << data.entries().size() << " genesis objects into new database";

      // Read genesis public key from the database, assert its existence at the correct location
      KOINOS_ASSERT( root->get_object( state::space::metadata(), state::key::genesis_key ),
                     unexpected_state_exception,
                     "could not find genesis public key in database" );

      // Calculate and write the chain ID into the database
      auto chain_id = crypto::hash( koinos::crypto::multicodec::sha2_256, data );
      LOG( info ) << "Calculated chain ID: " << chain_id;
      auto chain_id_str = util::converter::as< std::string >( chain_id );
      KOINOS_ASSERT( !root->get_object( chain::state::space::metadata(), chain::state::key::chain_id ),
                     unexpected_state_exception,
                     "encountered unexpected chain id in initial state" );

      root->put_object( chain::state::space::metadata(), chain::state::key::chain_id, &chain_id_str );
      LOG( info ) << "Wrote chain ID into new database";
    },
    comp,
    _db.get_unique_lock() );

  if( reset )
  {
    LOG( info ) << "Resetting database...";
    _db.reset( _db.get_unique_lock() );
  }

  auto head = _db.get_head( _db.get_shared_lock() );
  LOG( info ) << "Opened database at block - Height: " << head->revision() << ", ID: " << head->id();
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
  KOINOS_ASSERT( b.id().size(),
                 missing_required_arguments_exception,
                 "missing expected field in block: ${field}",
                 ( "field", "id" ) );
  KOINOS_ASSERT( b.has_header(),
                 missing_required_arguments_exception,
                 "missing expected field in block: ${field}",
                 ( "field", "header" )( "block_id", util::to_hex( b.id() ) ) );
  KOINOS_ASSERT( b.header().previous().size(),
                 missing_required_arguments_exception,
                 "missing expected field in block header: ${field}",
                 ( "field", "previous" )( "block_id", util::to_hex( b.id() ) ) );
  KOINOS_ASSERT( b.header().height(),
                 missing_required_arguments_exception,
                 "missing expected field in block header: ${field}",
                 ( "field", "height" )( "block_id", util::to_hex( b.id() ) ) );
  KOINOS_ASSERT( b.header().timestamp(),
                 missing_required_arguments_exception,
                 "missing expected field in block header: ${field}",
                 ( "field", "timestamp" )( "block_id", util::to_hex( b.id() ) ) );
  KOINOS_ASSERT( b.header().previous_state_merkle_root().size(),
                 missing_required_arguments_exception,
                 "missing expected field in block header: ${field}",
                 ( "field", "previous_state_merkle_root" )( "block_id", util::to_hex( b.id() ) ) );
  KOINOS_ASSERT( b.header().transaction_merkle_root().size(),
                 missing_required_arguments_exception,
                 "missing expected field in block header: ${field}",
                 ( "field", "transaction_merkle_root" )( "block_id", util::to_hex( b.id() ) ) );
  KOINOS_ASSERT( b.signature().size(),
                 missing_required_arguments_exception,
                 "missing expected field in block: ${field}",
                 ( "field", "signature_data" )( "block_id", util::to_hex( b.id() ) ) );

  for( const auto& t: b.transactions() )
    validate_transaction( t );
}

void controller_impl::validate_transaction( const protocol::transaction& t )
{
  KOINOS_ASSERT( t.id().size(),
                 missing_required_arguments_exception,
                 "missing expected field in transaction: ${field}",
                 ( "field", "id" ) );
  KOINOS_ASSERT( t.has_header(),
                 missing_required_arguments_exception,
                 "missing expected field in transaction: ${field}",
                 ( "field", "header" )( "transaction_id", util::to_hex( t.id() ) ) );
  KOINOS_ASSERT( t.header().rc_limit(),
                 missing_required_arguments_exception,
                 "missing expected field in transaction header: ${field}",
                 ( "field", "rc_limit" )( "transaction_id", util::to_hex( t.id() ) ) );
  KOINOS_ASSERT( t.header().operation_merkle_root().size(),
                 missing_required_arguments_exception,
                 "missing expected field in transaction header: ${field}",
                 ( "field", "operation_merkle_root" )( "transaction_id", util::to_hex( t.id() ) ) );
  KOINOS_ASSERT( t.signatures().size(),
                 missing_required_arguments_exception,
                 "missing expected field in transaction: ${field}",
                 ( "field", "signature_data" )( "transaction_id", util::to_hex( t.id() ) ) );
}

apply_block_result controller_impl::apply_block( const protocol::block& block, const apply_block_options& opts )
{
  validate_block( block );

  apply_block_result res;

  static constexpr uint64_t index_message_interval = 1'000;
  static constexpr std::chrono::seconds time_delta = std::chrono::seconds( 5 );
  static constexpr std::chrono::seconds live_delta = std::chrono::seconds( 60 );

  auto time_lower_bound = uint64_t( 0 );
  auto time_upper_bound =
    std::chrono::duration_cast< std::chrono::milliseconds >( ( opts.application_time + time_delta ).time_since_epoch() )
      .count();
  uint64_t parent_height = 0;

  auto db_lock = _db.get_shared_lock();

  auto block_id     = util::converter::to< crypto::multihash >( block.id() );
  auto block_height = block.header().height();
  auto parent_id    = util::converter::to< crypto::multihash >( block.header().previous() );
  auto block_node   = _db.get_node( block_id, db_lock );
  auto parent_node  = _db.get_node( parent_id, db_lock );

  bool new_head = false;

  if( block_node )
    return {}; // Block has been applied

  // This prevents returning "unknown previous block" when the pushed block is the LIB
  if( !parent_node )
  {
    auto root = _db.get_root( db_lock );
    KOINOS_ASSERT( block_height >= root->revision(),
                   pre_irreversibility_block_exception,
                   "block is prior to irreversibility" );
    KOINOS_ASSERT( block_id == root->id(), unknown_previous_block_exception, "unknown previous block" );
    return {}; // Block is current LIB
  }

  bool live = block.header().timestamp() > std::chrono::duration_cast< std::chrono::milliseconds >(
                                             ( opts.application_time - live_delta ).time_since_epoch() )
                                             .count();

  if( !opts.index_to && live )
  {
    LOG( debug ) << "Pushing block - Height: " << block_height << ", ID: " << block_id;
  }

  block_node = _db.create_writable_node( parent_id, block_id, block.header(), db_lock );

  // If this is not the genesis case, we must ensure that the proposed block timestamp is greater
  // than the parent block timestamp.
  if( block_node && !parent_id.is_zero() )
  {
    execution_context parent_ctx( _vm_backend, intent::read_only );

    parent_ctx.push_frame( stack_frame{ .call_privilege = privilege::kernel_mode } );

    parent_ctx.set_state_node( parent_node );
    parent_ctx.reset_cache();
    auto head_info   = system_call::get_head_info( parent_ctx );
    parent_height    = head_info.head_topology().height();
    time_lower_bound = head_info.head_block_time();
  }

  execution_context ctx( _vm_backend, opts.propose_block ? intent::block_proposal : intent::block_application );

  try
  {
    // Genesis case, when the first block is submitted the previous must be the zero hash
    if( parent_id.is_zero() )
    {
      KOINOS_ASSERT( block_height == 1, unexpected_height_exception, "first block must have height of 1" );
    }

    KOINOS_ASSERT( block_node, block_state_error_exception, "could not create new block state node" );

    KOINOS_ASSERT( block_height == parent_height + 1,
                   unexpected_height_exception,
                   "expected block height of ${a}, was ${b}",
                   ( "a", parent_height + 1 )( "b", block_height ) );

    KOINOS_ASSERT( block.header().timestamp() <= time_upper_bound,
                   timestamp_out_of_bounds_exception,
                   "block timestamp is too far in the future" );
    KOINOS_ASSERT( block.header().timestamp() > time_lower_bound,
                   timestamp_out_of_bounds_exception,
                   "block timestamp is too old" );

    KOINOS_ASSERT( block.header().previous_state_merkle_root()
                     == util::converter::as< std::string >( parent_node->merkle_root() ),
                   state_merkle_mismatch_exception,
                   "block previous state merkle mismatch" );

    ctx.push_frame( stack_frame{ .call_privilege = privilege::kernel_mode } );

    ctx.set_state_node( block_node );
    ctx.reset_cache();

    system_call::apply_block( ctx, block );

    res.failed_transaction_indices = ctx.get_failed_transaction_indices();

    if( opts.propose_block && res.failed_transaction_indices.size() )
    {
      // Icky, but the transaction failure code is in a catch block
      // so use the current flow of control

      KOINOS_THROW( failure_exception,
                    "${n} transactions failed in the block",
                    ( "n", res.failed_transaction_indices.size() ) );
    }

    KOINOS_ASSERT( std::holds_alternative< protocol::block_receipt >( ctx.receipt() ),
                   unexpected_receipt_exception,
                   "expected block receipt" );
    res.receipt = std::get< protocol::block_receipt >( ctx.receipt() );

    if( _client )
    {
      rpc::block_store::block_store_request req;
      req.mutable_add_block()->mutable_block_to_add()->CopyFrom( block );
      req.mutable_add_block()->mutable_receipt_to_add()->CopyFrom(
        std::get< protocol::block_receipt >( ctx.receipt() ) );

      auto future = _client->rpc( util::service::block_store,
                                  util::converter::as< std::string >( req ),
                                  1'500ms,
                                  mq::retry_policy::none );

      rpc::block_store::block_store_response resp;
      resp.ParseFromString( future.get() );

      KOINOS_ASSERT( !resp.has_error(),
                     rpc_failure_exception,
                     "received error from block store: ${e}",
                     ( "e", resp.error() ) );
      KOINOS_ASSERT( resp.has_add_block(),
                     rpc_failure_exception,
                     "unexpected response when submitting block: ${r}",
                     ( "r", resp ) );
    }

    if( !opts.index_to && live )
    {
      auto num_transactions = block.transactions_size();

      LOG( info ) << "Block applied - Height: " << block_height << ", ID: " << block_id << " (" << num_transactions
                  << ( num_transactions == 1 ? " transaction)" : " transactions)" );
    }
    else if( block_height % index_message_interval == 0 )
    {
      if( opts.index_to )
      {
        auto progress = block_height / static_cast< double >( opts.index_to ) * 100;
        LOG( info ) << "Indexing chain (" << progress << "%) - Height: " << block_height << ", ID: " << block_id;
      }
      else
      {
        auto to_go =
          std::chrono::duration_cast< std::chrono::seconds >(
            opts.application_time.time_since_epoch() - std::chrono::milliseconds( block.header().timestamp() ) )
            .count();
        LOG( info ) << "Sync progress - Height: " << block_height << ", ID: " << block_id << " ("
                    << format_time( to_go ) << " block time remaining)";
      }
    }

    auto lib = system_call::get_last_irreversible_block( ctx );

    try
    {
      // We need to finalize our node, checking if it is the new head block, update the cached head block,
      // and advancing LIB as an atomic action or else we risk _db.get_head(), _cached_head_block, and
      // LIB desyncing from each other
      db_lock.reset();
      block_node.reset();
      parent_node.reset();
      ctx.clear_state_node();

      auto unique_db_lock = _db.get_unique_lock();
      _db.finalize_node( block_id, unique_db_lock );

      res.receipt->set_state_merkle_root(
        util::converter::as< std::string >( _db.get_node( block_id, unique_db_lock )->merkle_root() ) );

      if( block_id == _db.get_head( unique_db_lock )->id() )
      {
        std::unique_lock< std::shared_mutex > head_lock( _cached_head_block_mutex );
        new_head           = true;
        _cached_head_block = std::make_shared< protocol::block >( block );
      }

      if( lib > _db.get_root( unique_db_lock )->revision() )
      {
        auto lib_id = _db.get_node_at_revision( lib, block_id, unique_db_lock )->id();
        _db.commit_node( lib_id, unique_db_lock );
      }

      unique_db_lock.reset();
      db_lock    = _db.get_shared_lock();
      block_node = _db.get_node( block_id, db_lock );
      ctx.set_state_node( block_node );
    }
    catch( ... )
    {
      // If any exception is thrown, reset to the expected local state and then rethrow.
      db_lock    = _db.get_shared_lock();
      block_node = _db.get_node( block_id, db_lock );
      ctx.set_state_node( block_node );
      throw;
    }

    // It is NOT safe to use block_node after this point without checking it against null

    if( _client )
    {
      const auto [ fork_heads, last_irreversible_block ] = get_fork_data( db_lock );

      broadcast::block_irreversible bc;
      bc.mutable_topology()->CopyFrom( last_irreversible_block );

      _client->broadcast( "koinos.block.irreversible", util::converter::as< std::string >( bc ) );

      broadcast::block_accepted ba;
      *ba.mutable_block()   = block;
      *ba.mutable_receipt() = std::get< protocol::block_receipt >( ctx.receipt() );
      ba.set_live( live );
      ba.set_head( new_head );

      _client->broadcast( "koinos.block.accept", util::converter::as< std::string >( ba ) );

      broadcast::fork_heads fh;
      fh.set_allocated_last_irreversible_block( bc.release_topology() );

      for( const auto& fork_head: fork_heads )
      {
        auto* head = fh.add_heads();
        *head      = fork_head;
      }

      _client->broadcast( "koinos.block.forks", util::converter::as< std::string >( fh ) );

      for( const auto& [ transaction_id, event ]: ctx.chronicler().events() )
      {
        broadcast::event_parcel ep;
        ep.set_block_id( block.id() );
        ep.set_height( block.header().height() );
        *ep.mutable_event() = event;

        if( transaction_id )
          ep.set_transaction_id( *transaction_id );

        _client->broadcast( "koinos.event." + util::to_base58( event.source() ) + "." + event.name(),
                            ep.SerializeAsString() );
      }
    }
  }
  catch( const block_state_error_exception& e )
  {
    LOG( warning ) << "Block application failed - Height: " << block_height << " ID: " << block_id
                   << ", with reason: " << e.what();
    throw;
  }
  catch( koinos::exception& e )
  {
    if( block_node && !block_node->is_finalized() )
    {
      _db.discard_node( block_node->id(), db_lock );
      LOG( warning ) << "Block application failed - Height: " << block_height << " ID: " << block_id
                     << ", with reason: " << e.what();
    }
    else
    {
      LOG( error ) << "Block application failed after finalization - Height: " << block_height << " ID: " << block_id
                   << ", with reason: " << e.what();
    }

    if( std::holds_alternative< protocol::block_receipt >( ctx.receipt() ) )
      e.add_json( "logs", std::get< protocol::block_receipt >( ctx.receipt() ).logs() );

    if( opts.propose_block && res.failed_transaction_indices.size() )
    {
      if( _client )
      {
        broadcast::transaction_failed trx_failed;

        for( auto i: res.failed_transaction_indices )
        {
          trx_failed.set_id( block.transactions( i ).id() );
          _client->broadcast( "koinos.transaction.fail", util::converter::as< std::string >( trx_failed ) );
        }
      }

      return res;
    }
    else if( _client )
    {
      const auto& exception_data = e.get_json();

      if( exception_data.count( "transaction_id" ) )
      {
        broadcast::transaction_failed ptf;
        ptf.set_id( util::from_hex< std::string >( exception_data[ "transaction_id" ] ) );
        _client->broadcast( "koinos.transaction.fail", util::converter::as< std::string >( ptf ) );
      }
    }

    throw;
  }
  catch( ... )
  {
    if( block_node && !block_node->is_finalized() )
    {
      _db.discard_node( block_node->id(), db_lock );
      LOG( warning ) << "Block application failed - Height: " << block_height << ", ID: " << block_id
                     << ", for an unknown reason";
    }
    else
    {
      LOG( error ) << "Block application failed after finalization - Height: " << block_height << ", ID: " << block_id
                   << ", for an unknown reason";
    }

    throw;
  }

  return res;
}

rpc::chain::submit_transaction_response
controller_impl::submit_transaction( const rpc::chain::submit_transaction_request& request )
{
  validate_transaction( request.transaction() );

  rpc::chain::submit_transaction_response resp;

  std::string payer, payee, nonce;
  uint64_t max_payer_rc;
  uint64_t trx_rc_limit;
  chain::value_type mempool_nonce;

  auto transaction    = request.transaction();
  auto transaction_id = util::to_hex( transaction.id() );

  LOG( debug ) << "Pushing transaction - ID: " << transaction_id;

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

  ctx.push_frame( stack_frame{ .call_privilege = privilege::kernel_mode } );

  try
  {
    ctx.reset_cache();

    payer        = transaction.header().payer();
    payee        = transaction.header().payee();
    nonce        = transaction.header().nonce();
    max_payer_rc = system_call::get_account_rc( ctx, payer );
    trx_rc_limit = transaction.header().rc_limit();

    if( request.broadcast() && _client )
    {
      rpc::mempool::mempool_request req1, req2, req3;
      auto* check_pending = req1.mutable_check_pending_account_resources();

      check_pending->set_payer( payer );
      check_pending->set_max_payer_rc( max_payer_rc );
      check_pending->set_rc_limit( trx_rc_limit );

      auto* check_nonce = req2.mutable_check_account_nonce();

      check_nonce->set_payee( payee.empty() ? payer : payee );
      check_nonce->set_nonce( nonce );

      auto* pending_nonce = req3.mutable_get_pending_nonce();

      pending_nonce->set_payee( payee.empty() ? payer : payee );

      auto future1 = _client->rpc( util::service::mempool,
                                   util::converter::as< std::string >( req1 ),
                                   750ms,
                                   mq::retry_policy::none );

      auto future2 = _client->rpc( util::service::mempool,
                                   util::converter::as< std::string >( req2 ),
                                   750ms,
                                   mq::retry_policy::none );

      auto future3 = _client->rpc( util::service::mempool,
                                   util::converter::as< std::string >( req3 ),
                                   750ms,
                                   mq::retry_policy::none );

      rpc::mempool::mempool_response resp;
      resp.ParseFromString( future1.get() );

      KOINOS_ASSERT( !resp.has_error(),
                     rpc_failure_exception,
                     "received error from mempool: ${e}",
                     ( "e", resp.error() ) );
      KOINOS_ASSERT( resp.has_check_pending_account_resources(),
                     rpc_failure_exception,
                     "received unexpected response from mempool" );
      KOINOS_ASSERT( resp.check_pending_account_resources().success(),
                     insufficient_rc_exception,
                     "insufficient pending account resources" );

      resp.ParseFromString( future2.get() );

      KOINOS_ASSERT( !resp.has_error(),
                     rpc_failure_exception,
                     "received error from mempool: ${e}",
                     ( "e", resp.error() ) );
      KOINOS_ASSERT( resp.has_check_account_nonce(),
                     rpc_failure_exception,
                     "received unexpected response from mempool" );
      KOINOS_ASSERT( resp.check_account_nonce().success(), invalid_nonce_exception, "invalid account nonce" );

      resp.ParseFromString( future3.get() );
      KOINOS_ASSERT( !resp.has_error(),
                     rpc_failure_exception,
                     "received error from mempool: ${e}",
                     ( "e", resp.error() ) );
      KOINOS_ASSERT( resp.has_get_pending_nonce(), rpc_failure_exception, "received unexpected response from mempool" );
      mempool_nonce = util::converter::to< chain::value_type >( resp.get_pending_nonce().nonce() );

      if( mempool_nonce.has_uint64_value() )
        ctx.set_mempool_nonce( mempool_nonce );
    }

    ctx.resource_meter().set_resource_limit_data( system_call::get_resource_limits( ctx ) );
    system_call::apply_transaction( ctx, transaction );

    LOG( debug ) << "Transaction applied - ID: " << transaction_id;

    KOINOS_ASSERT( std::holds_alternative< protocol::transaction_receipt >( ctx.receipt() ),
                   unexpected_receipt_exception,
                   "expected transaction receipt" );
    *resp.mutable_receipt() = std::get< protocol::transaction_receipt >( ctx.receipt() );

    if( request.broadcast() && _client )
    {
      broadcast::transaction_accepted ta;
      *ta.mutable_transaction() = transaction;
      *ta.mutable_receipt()     = std::get< protocol::transaction_receipt >( ctx.receipt() );
      ta.set_height( ctx.get_state_node()->revision() );

      _client->broadcast( "koinos.transaction.accept", util::converter::as< std::string >( ta ) );
    }
  }
  catch( koinos::exception& e )
  {
    LOG( debug ) << "Transaction application failed - ID: " << transaction_id << ", with reason: " << e.what();

    if( std::holds_alternative< protocol::transaction_receipt >( ctx.receipt() ) )
      e.add_json( "logs", std::get< protocol::transaction_receipt >( ctx.receipt() ).logs() );

    throw;
  }
  catch( ... )
  {
    LOG( debug ) << "Transaction application failed - ID: " << transaction_id << ", for an unknown reason";
    throw;
  }

  return resp;
}

rpc::chain::get_head_info_response controller_impl::get_head_info( const rpc::chain::get_head_info_request& )
{
  execution_context ctx( _vm_backend );
  ctx.push_frame( stack_frame{ .call_privilege = privilege::kernel_mode } );

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

  auto head_info      = system_call::get_head_info( ctx );
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
  ctx.push_frame( stack_frame{ .call_privilege = privilege::kernel_mode } );

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

  ctx.push_frame( koinos::chain::stack_frame{ .call_privilege = privilege::kernel_mode } );

  std::vector< state_db::state_node_ptr > fork_heads;

  ctx.set_state_node( _db.get_root( db_lock )->create_anonymous_node() );
  ctx.reset_cache();
  fork_heads = _db.get_fork_heads( db_lock );

  auto head_info = system_call::get_head_info( ctx );
  fdata.second   = head_info.head_topology();

  for( auto& fork: fork_heads )
  {
    ctx.set_state_node( fork->create_anonymous_node() );
    ctx.reset_cache();
    auto head_info = system_call::get_head_info( ctx );
    fdata.first.emplace_back( std::move( head_info.head_topology() ) );
  }

  // Sort all fork heads by height
  std::sort( fdata.first.begin(),
             fdata.first.end(),
             []( const block_topology& a, const block_topology& b )
             {
               return a.height() > b.height();
             } );

  // If there is a tie for highest block, ensure the head block is first
  auto fork_itr = fdata.first.begin();
  while( fork_itr != fdata.first.begin() && fork_itr->id() != head_info.head_topology().id() )
  {
    ++fork_itr;
  }

  if( fork_itr != fdata.first.begin() )
  {
    std::iter_swap( fork_itr, fdata.first.begin() );
  }

  return fdata;
}

rpc::chain::get_resource_limits_response
controller_impl::get_resource_limits( const rpc::chain::get_resource_limits_request& )
{
  execution_context ctx( _vm_backend );
  ctx.push_frame( stack_frame{ .call_privilege = privilege::kernel_mode } );

  ctx.set_state_node( _db.get_head( _db.get_shared_lock() )->create_anonymous_node() );
  ctx.reset_cache();

  auto value = system_call::get_resource_limits( ctx );

  rpc::chain::get_resource_limits_response resp;
  *resp.mutable_resource_limit_data() = value;

  return resp;
}

rpc::chain::get_account_rc_response controller_impl::get_account_rc( const rpc::chain::get_account_rc_request& request )
{
  KOINOS_ASSERT( request.account().size(),
                 missing_required_arguments_exception,
                 "missing expected field: ${f}",
                 ( "f", "payer" ) );

  execution_context ctx( _vm_backend );
  ctx.push_frame( stack_frame{ .call_privilege = privilege::kernel_mode } );

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
  auto topo                                          = resp.mutable_last_irreversible_block();
  *topo                                              = std::move( last_irreversible_block );

  for( const auto& fork_head: fork_heads )
  {
    auto* head = resp.add_fork_heads();
    *head      = fork_head;
  }

  return resp;
}

rpc::chain::read_contract_response controller_impl::read_contract( const rpc::chain::read_contract_request& request )
{
  KOINOS_ASSERT( request.contract_id().size(),
                 missing_required_arguments_exception,
                 "missing expected field: ${f}",
                 ( "f", "contract_id" ) );

  auto db_lock = _db.get_shared_lock();

  execution_context ctx( _vm_backend, intent::read_only );
  ctx.push_frame( stack_frame{
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

  try
  {
    resp.set_result( system_call::call( ctx, request.contract_id(), request.entry_point(), request.args() ) );
  }
  catch( koinos::exception& e )
  {
    e.add_json( "logs", ctx.chronicler().logs() );
    throw e;
  }

  for( const auto& message: ctx.chronicler().logs() )
    *resp.add_logs() = message;

  return resp;
}

rpc::chain::get_account_nonce_response
controller_impl::get_account_nonce( const rpc::chain::get_account_nonce_request& request )
{
  KOINOS_ASSERT( request.account().size(),
                 missing_required_arguments_exception,
                 "missing expected field: ${f}",
                 ( "f", "account" ) );

  execution_context ctx( _vm_backend );

  ctx.push_frame( koinos::chain::stack_frame{ .call_privilege = privilege::kernel_mode } );

  ctx.set_state_node( _db.get_head( _db.get_shared_lock() )->create_anonymous_node() );
  ctx.reset_cache();

  auto nonce = system_call::get_account_nonce( ctx, request.account() );

  rpc::chain::get_account_nonce_response resp;
  resp.set_nonce( nonce );

  return resp;
}

rpc::chain::invoke_system_call_response
controller_impl::invoke_system_call( const rpc::chain::invoke_system_call_request& request )
{
  KOINOS_ASSERT( request.has_id() || request.has_name(),
                 missing_required_arguments_exception,
                 "missing expected field: ${f1} or ${f2}",
                 ( "f1", "id" )( "f2", "name" ) );

  execution_context ctx( _vm_backend, intent::read_only );

  stack_frame sframe;

  if( request.has_caller_data() )
  {
    sframe.contract_id    = request.caller_data().caller();
    sframe.call_privilege = request.caller_data().caller_privilege();
  }
  else
  {
    sframe.call_privilege = privilege::kernel_mode;
  }

  ctx.push_frame( std::move( sframe ) );

  ctx.set_state_node( _db.get_head( _db.get_shared_lock() )->create_anonymous_node() );
  ctx.reset_cache();

  resource_limit_data rl;
  rl.set_compute_bandwidth_limit( _read_compute_bandwidth_limit );

  ctx.resource_meter().set_resource_limit_data( rl );

  system_call_id syscall_id;

  if( request.has_id() )
  {
    syscall_id = system_call_id( request.id() );
  }
  else
  {
    if( !system_call_id_Parse( request.name(), &syscall_id ) )
      KOINOS_THROW( unknown_system_call_exception, "unknown system call name" );
  }

  koinos::chain::host_api hapi( ctx );

  std::vector< char > buffer( _syscall_bufsize, 0 );
  uint32_t bytes_written;
  rpc::chain::invoke_system_call_response resp;

  hapi.call( syscall_id,
             &buffer[ 0 ],
             _syscall_bufsize,
             request.args().c_str(),
             uint32_t( request.args().size() ),
             &bytes_written );

  resp.set_value( std::string( &buffer[ 0 ], bytes_written ) );

  return resp;
}

} // namespace detail

controller::controller( uint64_t read_compute_bandwith_limit, uint32_t syscall_bufsize ):
    _my( std::make_unique< detail::controller_impl >( read_compute_bandwith_limit, syscall_bufsize ) )
{}

controller::~controller() = default;

void controller::open( const std::filesystem::path& p,
                       const chain::genesis_data& data,
                       fork_resolution_algorithm algo,
                       bool reset )
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

rpc::chain::submit_block_response controller::submit_block( const rpc::chain::submit_block_request& request,
                                                            uint64_t index_to,
                                                            std::chrono::system_clock::time_point now )
{
  rpc::chain::submit_block_response resp;

  auto res = _my->apply_block( request.block(), detail::apply_block_options{ index_to, now, false } );

  if( res.receipt )
    *resp.mutable_receipt() = res.receipt.value();

  return resp;
}

rpc::chain::propose_block_response controller::propose_block( const rpc::chain::propose_block_request& request,
                                                              uint64_t index_to,
                                                              std::chrono::system_clock::time_point now )
{
  rpc::chain::propose_block_response resp;

  auto res = _my->apply_block( request.block(), detail::apply_block_options{ index_to, now, true } );

  if( res.receipt )
    *resp.mutable_receipt() = res.receipt.value();
  else
  {
    for( auto i: res.failed_transaction_indices )
    {
      resp.add_failed_transaction_indices( i );
    }
  }

  return resp;
}

rpc::chain::submit_transaction_response
controller::submit_transaction( const rpc::chain::submit_transaction_request& request )
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

rpc::chain::get_account_nonce_response
controller::get_account_nonce( const rpc::chain::get_account_nonce_request& request )
{
  return _my->get_account_nonce( request );
}

rpc::chain::get_account_rc_response controller::get_account_rc( const rpc::chain::get_account_rc_request& request )
{
  return _my->get_account_rc( request );
}

rpc::chain::get_resource_limits_response
controller::get_resource_limits( const rpc::chain::get_resource_limits_request& request )
{
  return _my->get_resource_limits( request );
}

rpc::chain::invoke_system_call_response
controller::invoke_system_call( const rpc::chain::invoke_system_call_request& request )
{
  return _my->invoke_system_call( request );
}

} // namespace koinos::chain
