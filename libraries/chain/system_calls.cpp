#include <algorithm>
#include <string>

#include <google/protobuf/message.h>

#include <koinos/bigint.hpp>
#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/constants.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/log.hpp>

using namespace std::string_literals;

namespace koinos::chain {

void register_thunks( thunk_dispatcher& td )
{
   THUNK_REGISTER( td,
      (prints)
      (exit_contract)

      (verify_block_signature)
      (verify_merkle_root)

      (apply_block)
      (apply_transaction)
      (apply_upload_contract_operation)
      (apply_call_contract_operation)
      (apply_set_system_call_operation)

      (put_object)
      (get_object)
      (get_next_object)
      (get_prev_object)

      (call_contract)

      (get_entry_point)
      (get_contract_arguments_size)
      (get_contract_arguments)
      (set_contract_result)

      (get_head_info)
      (hash)
      (recover_public_key)

      (get_transaction_payer)
      (get_transaction_rc_limit)

      (get_last_irreversible_block)

      (get_caller)
      (get_transaction_signature)
      (require_authority)

      (get_contract_id)

      (get_account_nonce)

      (get_account_rc)
      (consume_account_rc)
      (get_resource_limits)
      (consume_block_resources)
   )
}

KOINOS_TODO( "Should this be a thunk?" );
KOINOS_TODO( "This is called on every database access. Can we optimize away the conversion because it is done all the time?" );
bool is_system_space( const state_db::object_space& space_id )
{
   return space_id == converter::as< state_db::object_space >( database::space::contract ) ||
          space_id == converter::as< state_db::object_space >( database::space::system_call_dispatch ) ||
          space_id == converter::as< state_db::object_space >( database::space::kernel );
}

THUNK_DEFINE_BEGIN();

THUNK_DEFINE( void, prints, ((const std::string&) str) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );
   context.console_append( str );
}

THUNK_DEFINE( void, exit_contract, ((uint32_t) exit_code) )
{
   switch( exit_code )
   {
      case KOINOS_EXIT_SUCCESS:
          KOINOS_THROW( exit_success, "" );
      case KOINOS_EXIT_FAILURE:
          KOINOS_THROW( exit_failure, "" );
      default:
          KOINOS_THROW( unknown_exit_code, "contract specified unknown exit code" );
   }
}

THUNK_DEFINE( verify_block_signature_result, verify_block_signature, ((const std::string&) id, (const std::string&) active_data, (const std::string&) signature_data) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   crypto::multihash chain_id;
   crypto::recoverable_signature sig;
   std::memcpy( sig.data(), signature_data.c_str(), signature_data.size() );
   crypto::multihash block_id = converter::to< crypto::multihash >( id );

   with_stack_frame(
      context,
      stack_frame {
         .call = crypto::hash( crypto::multicodec::ripemd_160, "retrieve_chain_id"s ).digest(),
         .call_privilege = privilege::kernel_mode,
      },
      [&]() {
         auto obj = system_call::get_object( context, database::space::kernel, database::key::chain_id ).value();
         chain_id = converter::to< crypto::multihash >( obj );
      }
   );

   verify_block_signature_result ret;
   ret.set_value( chain_id == crypto::hash( crypto::multicodec::sha2_256, crypto::public_key::recover( sig, block_id ).to_address_bytes() ) );
   return ret;
}

THUNK_DEFINE( verify_merkle_root_result, verify_merkle_root, ((const std::string&) root, (const std::vector< std::string >&) hashes) )
{
   uint128_t compute_bandwidth = uint128_t( compute_load::light ) * hashes.size();
   compute_bandwidth = compute_bandwidth > std::numeric_limits< uint64_t >::max() ? std::numeric_limits< uint64_t >::max() : compute_bandwidth;
   context.resource_meter().use_compute_bandwidth( compute_bandwidth.convert_to< uint64_t >() );

   std::vector< crypto::multihash > leaves;

   leaves.resize( hashes.size() );
   std::transform( std::begin( hashes ), std::end( hashes ), std::begin( leaves ), []( const std::string& s ) { return converter::to< crypto::multihash >( s ); } );

   auto root_hash = converter::to< crypto::multihash >( root );
   auto mtree = crypto::merkle_tree( root_hash.code(), leaves );

   auto merkle_root = mtree.root()->hash();

   verify_merkle_root_result ret;
   ret.set_value( merkle_root == root_hash );
   return ret;
}

// RAII class to ensure apply context block state is consistent if there is an error applying
// the block.
struct block_setter
{
   block_setter( apply_context& context, const protocol::block& block ) :
      ctx( context )
   {
      ctx.set_block( block );
   }

   ~block_setter()
   {
      ctx.clear_block();
   }

   apply_context& ctx;
};

THUNK_DEFINE( void, apply_block,
   (
      (const protocol::block&) block,
      (bool) check_passive_data,
      (bool) check_block_signature,
      (bool) check_transaction_signatures)
   )
{
   KOINOS_TODO( "Check previous block hash, height, timestamp, and specify allowed set of hashing algorithms" );

   KOINOS_ASSERT( !context.is_in_user_code(), insufficient_privileges, "calling privileged thunk from non-privileged code" );

   auto setter = block_setter( context, block );

   context.resource_meter().set_resource_limit_data( system_call::get_resource_limits( context ).value() );

   protocol::active_block_data active_data;
   active_data.ParseFromString( block.active() );
   const crypto::multihash tx_root = converter::to< crypto::multihash >( active_data.transaction_merkle_root() );
   size_t tx_count = block.transactions_size();

   // Check transaction Merkle root
   std::vector< std::string > hashes;
   hashes.reserve( tx_count );

   std::size_t transactions_bytes_size = 0;
   for ( const auto& trx : block.transactions() )
   {
      transactions_bytes_size += trx.ByteSizeLong();
      hashes.emplace_back( converter::as< std::string >( crypto::hash( tx_root.code(), trx.active() ) ) );
   }

   context.resource_meter().use_network_bandwidth( block.ByteSizeLong() - transactions_bytes_size );

   KOINOS_ASSERT( system_call::verify_merkle_root( context, active_data.transaction_merkle_root(), hashes ).value(), transaction_root_mismatch, "transaction merkle root does not match" );

   /*
    * The PoW implementation of verify_block_signature has side effects. While this is the case, we should never
    * skip it. We either need to remove this flag or redesign our system call arhictecture the prevent
    * side effects within verify_block_signature. (Issue 408)
    */
   KOINOS_TODO( "Rearchitect verify_block_signature or remove check_block_signature flag. (Issue #408)" )
   // if( check_block_signature )
   {
      crypto::multihash block_hash = crypto::hash( tx_root.code(), block.header(), block.active() );
      KOINOS_ASSERT( system_call::verify_block_signature( context, converter::as< std::string >( block_hash ), block.active(), block.signature_data() ).value(), invalid_block_signature, "block signature does not match" );
   }

   system_call::put_object( context, database::space::kernel, database::key::head_block_time, converter::as< std::string >( block.header().timestamp() ) );

   // Check passive Merkle root
   if( check_passive_data )
   {
      // Passive Merkle root verifies:
      //
      // Block passive
      // Block signature slot (zero hash)
      // Transaction signatures
      //
      // Transaction passive
      // Transaction signature
      //
      // This matches the pattern of the input, except the hash of block_sig is zero because it has not yet been determined
      // during the block building process.

      KOINOS_TODO( "Can we optimize away the string copies?" );
      auto passive_root = converter::to< crypto::multihash >( active_data.passive_data_merkle_root() );
      std::vector< std::string > passives;
      passives.reserve( 2 * ( block.transactions().size() + 1 ) );

      passives.emplace_back( converter::as< std::string >( crypto::hash( passive_root.code(), block.passive() ) ) );
      passives.emplace_back( converter::as< std::string >( crypto::multihash::empty( passive_root.code() ) ) );

      for ( const auto& trx : block.transactions() )
      {
         passives.emplace_back( converter::as< std::string >( crypto::hash( passive_root.code(), trx.passive() ) ) );
         passives.emplace_back( converter::as< std::string >( crypto::hash( passive_root.code(), trx.signature_data() ) ) );
      }

      KOINOS_ASSERT( system_call::verify_merkle_root( context, active_data.passive_data_merkle_root(), passives ).value(), passive_root_mismatch, "passive merkle root does not match" );
   }

   //
   // +-----------+      +--------------+      +-------------------------+      +---------------------+
   // | Block sig | ---> | Block active | ---> | Transaction merkle root | ---> | Transaction actives |
   // +-----------+      +--------------+      +-------------------------+      +---------------------+
   //                           |
   //                           V
   //                +----------------------+      +----------------------+
   //                |                      | ---> |     Block passive    |
   //                |                      |      +----------------------+
   //                |                      |
   //                |                      |      +----------------------+
   //                | Passives merkle root | ---> | Transaction passives |
   //                |                      |      +----------------------+
   //                |                      |
   //                |                      |      +----------------------+
   //                |                      | ---> |   Transaction sigs   |
   //                +----------------------+      +----------------------+
   //

   for ( const auto& tx : block.transactions() )
   {
      try
      {
         system_call::apply_transaction( context, tx );
      }
      catch( const transaction_reverted& ) {} /* do nothing */
   }

   KOINOS_ASSERT(
      system_call::consume_block_resources(
         context,
         context.resource_meter().disk_storage_used(),
         context.resource_meter().network_bandwidth_used(),
         context.resource_meter().compute_bandwidth_used()
      ).value(),
      unable_to_consume_resources,
      "unable to consume block resources"
   );
}

// RAII class to ensure apply context transaction state is consistent if there is an error applying
// the transaction.
struct transaction_setter
{
   transaction_setter( apply_context& context, const protocol::transaction& trx ) :
      ctx( context )
   {
      ctx.set_transaction( trx );
   }

   ~transaction_setter()
   {
      ctx.clear_transaction();
   }

   apply_context& ctx;
};

inline void require_payer_transaction_nonce( apply_context& ctx, const std::string& payer, uint64_t nonce )
{
   auto account_nonce = system_call::get_account_nonce( ctx, payer ).value();
   KOINOS_ASSERT(
      account_nonce == nonce,
      chain::chain_exception,
      "mismatching transaction nonce - trx nonce: ${d}, expected: ${e}", ("d", nonce)("e", account_nonce)
   );
}

inline void update_payer_transaction_nonce( apply_context& ctx, const std::string& payer, uint64_t nonce )
{
   KOINOS_ASSERT(
      system_call::put_object( ctx, database::space::kernel, database::key::transaction_nonce( payer ), converter::as< std::string >( nonce ) ).value(),
      nonce_update_failure,
      "unable to update payer nonce for: ${p}", ("p", payer)
   );
}

THUNK_DEFINE( void, apply_transaction, ((const protocol::transaction&) trx) )
{
   KOINOS_ASSERT( !context.is_in_user_code(), insufficient_privileges, "calling privileged thunk from non-privileged code" );

   auto setter = transaction_setter( context, trx );

   protocol::active_transaction_data active_data;
   KOINOS_ASSERT(
      active_data.ParseFromString( trx.active() ),
      koinos::exception,
      "unable to parse transaction active data"
   );

   std::string payer = system_call::get_transaction_payer( context, trx ).value();

   auto payer_rc = system_call::get_account_rc( context, payer ).value();
   KOINOS_ASSERT( payer_rc >= active_data.rc_limit(), insufficient_rc, "payer does not have the rc to cover transaction rc limit" );

   /**
    * While a reference to the payer_session remains alive, resource usage will be tallied
    * and charged to the current payer.
    */
   auto payer_session = context.resource_meter().make_session( active_data.rc_limit() );

   require_payer_transaction_nonce( context, payer, active_data.nonce() );
   system_call::require_authority( context, payer );

   auto block_node = context.get_state_node();
   auto trx_node = block_node->create_anonymous_node();
   context.set_state_node( trx_node, block_node->get_parent() );

   std::exception_ptr reverted_exception_ptr;

   try
   {
      context.resource_meter().use_network_bandwidth( trx.ByteSizeLong() );

      for ( const auto& o : active_data.operations() )
      {
         if ( o.has_upload_contract() )
            system_call::apply_upload_contract_operation( context, o.upload_contract() );
         else if ( o.has_call_contract() )
            system_call::apply_call_contract_operation( context, o.call_contract() );
         else if ( o.has_set_system_call() )
            system_call::apply_set_system_call_operation( context, o.set_system_call() );
         else
            KOINOS_THROW( unknown_operation, "unknown operation" );
      }

      trx_node->commit();
   }
   catch ( const block_resource_limit_exceeded& e )
   {
      throw e;
   }
   catch ( const std::exception& e )
   {
      LOG(info) << "Transaction " << to_hex( trx.id() ) << " reverted with: " << e.what();
      transaction_reverted tr( e.what() );
      reverted_exception_ptr = std::make_exception_ptr( tr );
   }

   context.set_state_node( block_node );

   // Next nonce should be the current nonce + 1
   update_payer_transaction_nonce( context, payer, active_data.nonce() + 1 );

   auto payer_consumed_rc = payer_session->used();
   payer_session.reset();

   KOINOS_ASSERT(
      system_call::consume_account_rc( context, payer, payer_consumed_rc ).value(),
      unable_to_consume_resources,
      "unable to consume rc for payer: ${p}", ("p", to_hex( payer ) )
   );

   if ( reverted_exception_ptr )
   {
      std::rethrow_exception( reverted_exception_ptr );
   }

   LOG(debug) << "(apply_transaction) transaction " << trx.id();
}

THUNK_DEFINE( void, apply_upload_contract_operation, ((const protocol::upload_contract_operation&) o) )
{
   KOINOS_ASSERT( !context.is_in_user_code(), insufficient_privileges, "calling privileged thunk from non-privileged code" );

   context.resource_meter().use_compute_bandwidth( compute_load::medium );

   protocol::active_transaction_data active_data;
   active_data.ParseFromString( context.get_transaction().active() );

   auto tx_id       = crypto::hash( crypto::multicodec::sha2_256, active_data );
   auto sig_account = system_call::recover_public_key( context, get_transaction_signature( context ).value(), converter::as< std::string >( tx_id ) ).value();
   auto signer_hash = crypto::hash( crypto::multicodec::ripemd_160, sig_account );
   auto contract_id = converter::to< crypto::multihash >( o.contract_id() );

   KOINOS_ASSERT(
      signer_hash == contract_id,
      invalid_signature,
      "signature does not match: ${contract_id} != ${signer_hash}", ("contract_id", contract_id)("signer_hash", signer_hash)
   );

   system_call::put_object( context, database::space::contract, o.contract_id(), o.bytecode() );
}

THUNK_DEFINE( void, apply_call_contract_operation, ((const protocol::call_contract_operation&) o) )
{
   KOINOS_ASSERT( !context.is_in_user_code(), insufficient_privileges, "calling privileged thunk from non-privileged code" );

   context.resource_meter().use_compute_bandwidth( compute_load::light );

   with_stack_frame(
      context,
      stack_frame {
         .call = crypto::hash( crypto::multicodec::ripemd_160, "apply_call_contract_operation"s ).digest(),
         .call_privilege = privilege::user_mode,
      },
      [&]() {
         system_call::call_contract( context, o.contract_id(), o.entry_point(), o.args() );
      }
   );
}

THUNK_DEFINE( void, apply_set_system_call_operation, ((const protocol::set_system_call_operation&) o) )
{
   KOINOS_ASSERT( !context.is_in_user_code(), insufficient_privileges, "calling privileged thunk from non-privileged code" );

   context.resource_meter().use_compute_bandwidth( compute_load::heavy );

   crypto::multihash chain_id;

   with_stack_frame(
      context,
      stack_frame {
         .call = crypto::hash( crypto::multicodec::ripemd_160, "retrieve_chain_id"s ).digest(),
         .call_privilege = privilege::kernel_mode,
      },
      [&]() {
         auto obj = system_call::get_object( context, database::space::kernel, database::key::chain_id ).value();
         chain_id = converter::to< crypto::multihash >( obj );
      }
   );

   const auto& tx = context.get_transaction();
   crypto::recoverable_signature sig;
   std::memcpy( sig.data(), tx.signature_data().c_str(), tx.signature_data().size() );

   KOINOS_ASSERT(
      chain_id == crypto::hash( crypto::multicodec::sha2_256, crypto::public_key::recover( sig, converter::to< crypto::multihash >( tx.id() ) ).to_address_bytes() ),
      insufficient_privileges,
      "transaction does not have the required authority to override system calls"
   );

   if ( o.target().has_system_call_bundle() )
   {
      auto contract_id = converter::to< crypto::multihash >( o.target().system_call_bundle().contract_id() );
      auto contract = system_call::get_object( context, database::space::contract, converter::as< std::string >( contract_id ) ).value();
      KOINOS_ASSERT( contract.size(), invalid_contract, "contract does not exist" );
      KOINOS_ASSERT( ( o.call_id() != protocol::system_call_id::call_contract ), forbidden_override, "cannot override call_contract" );

      LOG(info) << "Overriding system call " << o.call_id() << " with contract " << to_hex( o.target().system_call_bundle().contract_id() ) << " at entry point " << o.target().system_call_bundle().entry_point();
   }
   else if ( o.target().thunk_id() )
   {
      KOINOS_ASSERT( thunk_dispatcher::instance().thunk_exists( o.target().thunk_id() ), thunk_not_found, "thunk ${tid} does not exist", ("tid", o.target().thunk_id()) );
      LOG(info) << "Overriding system call " << o.call_id() << " with thunk " << o.target().thunk_id();
   }
   else
   {
      KOINOS_THROW( unknown_system_call, "set_system_call invoked with unknown override" );
   }

   // Place the override in the database
   system_call::put_object( context, database::space::system_call_dispatch, converter::as< std::string >( std::underlying_type_t< koinos::protocol::system_call_id >( o.call_id() ) ), converter::as< std::string >( o.target() ) );
}

void check_db_permissions( const apply_context& context, const state_db::object_space& space )
{
   auto privilege = context.get_privilege();
   auto caller = converter::to< state_db::object_space >( context.get_caller() );
   if ( space != caller )
   {
      if ( context.get_privilege() == privilege::kernel_mode )
      {
         KOINOS_ASSERT( is_system_space( space ), insufficient_privileges, "privileged code can only accessed system space" );
      }
      else
      {
         KOINOS_THROW( out_of_bounds, "contract attempted access of non-contract database space" );
      }
   }
}

THUNK_DEFINE( put_object_result, put_object, ((const std::string&) space, (const std::string&) key, (const std::string&) obj) )
{
   KOINOS_ASSERT( !context.is_read_only(), read_only_context, "cannot put object during read only call" );

   context.resource_meter().use_disk_storage( obj.size() );
   context.resource_meter().use_compute_bandwidth( compute_load::medium );

   const auto _space = converter::as< state_db::object_space >( space );
   const auto _key   = converter::as< state_db::object_key >( key );
   const auto _obj   = converter::as< state_db::object_value >( obj );

   check_db_permissions( context, _space );

   auto state = context.get_state_node();
   KOINOS_ASSERT( state, state_node_not_found, "current state node does not exist" );
   state_db::put_object_args put_args;
   put_args.space = _space;
   put_args.key = _key;
   put_args.buf = _obj.data();
   put_args.object_size = _obj.size();

   state_db::put_object_result put_res;
   state->put_object( put_res, put_args );

   put_object_result ret;
   ret.set_value( put_res.object_existed );
   return ret;
}

THUNK_DEFINE( get_object_result, get_object, ((const std::string&) space, (const std::string&) key, (uint32_t) object_size_hint) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::medium );

   const auto _space = converter::as< state_db::object_space >( space );
   const auto _key   = converter::as< state_db::object_key >( key );

   check_db_permissions( context, _space );

   abstract_state_node_ptr state = space == database::space::system_call_dispatch ? context.get_parent_node() : context.get_state_node();

   KOINOS_ASSERT( state, state_node_not_found, "current state node does not exist" );

   state_db::get_object_args get_args;
   get_args.space = _space;
   get_args.key = _key;
   get_args.buf_size = object_size_hint > 0 ? object_size_hint : database::max_object_size;

   state_db::object_value object_buffer;
   object_buffer.resize( get_args.buf_size );
   get_args.buf = object_buffer.data();

   state_db::get_object_result get_res;
   state->get_object( get_res, get_args );

   if( get_res.key == get_args.key && get_res.size > 0 )
      object_buffer.resize( get_res.size );
   else
      object_buffer.clear();

   get_object_result ret;
   ret.set_value( converter::to< std::string >( object_buffer ) );
   return ret;
}

THUNK_DEFINE( get_next_object_result, get_next_object, ((const std::string&) space, (const std::string&) key, (uint32_t) object_size_hint) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::medium );

   const auto _space = converter::as< state_db::object_space >( space );
   const auto _key   = converter::as< state_db::object_key >( key );

   check_db_permissions( context, _space );

   abstract_state_node_ptr state = space == database::space::system_call_dispatch ? context.get_parent_node() : context.get_state_node();
   KOINOS_ASSERT( state, state_node_not_found, "current state node does not exist" );
   state_db::get_object_args get_args;
   get_args.space = _space;
   get_args.key = _key;
   get_args.buf_size = object_size_hint > 0 ? object_size_hint : database::max_object_size;

   state_db::object_value object_buffer;
   object_buffer.resize( get_args.buf_size );
   get_args.buf = object_buffer.data();

   state_db::get_object_result get_res;
   state->get_next_object( get_res, get_args );

   if( get_res.size > 0 )
      object_buffer.resize( get_res.size );
   else
      object_buffer.clear();

   get_next_object_result ret;
   ret.set_value( converter::to< std::string >( object_buffer ) );
   return ret;
}

THUNK_DEFINE( get_prev_object_result, get_prev_object, ((const std::string&) space, (const std::string&) key, (uint32_t) object_size_hint) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::medium );

   const auto _space = converter::as< state_db::object_space >( space );
   const auto _key   = converter::as< state_db::object_key >( key );

   check_db_permissions( context, _space );

   abstract_state_node_ptr state = space == database::space::system_call_dispatch ? context.get_parent_node() : context.get_state_node();
   KOINOS_ASSERT( state, state_node_not_found, "current state node does not exist" );
   state_db::get_object_args get_args;
   get_args.space = _space;
   get_args.key = _key;
   get_args.buf_size = object_size_hint > 0 ? object_size_hint : database::max_object_size;

   state_db::object_value object_buffer;
   object_buffer.resize( get_args.buf_size );
   get_args.buf = object_buffer.data();

   state_db::get_object_result get_res;
   state->get_prev_object( get_res, get_args );

   if( get_res.size > 0 )
      object_buffer.resize( get_res.size );
   else
      object_buffer.clear();

   get_prev_object_result ret;
   ret.set_value( converter::to< std::string >( object_buffer ) );
   return ret;
}

THUNK_DEFINE( call_contract_result, call_contract, ((const std::string&) contract_id, (uint32_t) entry_point, (const std::string&) args) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::medium );

   // We need to be in kernel mode to read the contract data
   std::string bytecode;
   with_stack_frame(
      context,
      stack_frame {
         .call = crypto::hash( crypto::multicodec::ripemd_160, "call_contract"s ).digest(),
         .call_privilege = privilege::kernel_mode,
      },
      [&]()
      {
         bytecode = system_call::get_object( context, database::space::contract, contract_id ).value();
         KOINOS_ASSERT( bytecode.size(), invalid_contract, "contract does not exist" );
      }
   );

   context.push_frame( stack_frame {
      .call = converter::as< std::vector< std::byte > >( contract_id ),
      .call_privilege = context.get_privilege(),
      .call_args = converter::as< std::vector< std::byte > >( args ),
      .entry_point = entry_point
   } );

   chain::host_api hapi( context );

   try
   {
      context.get_backend()->run( hapi, bytecode.data(), bytecode.size() );
   }
   catch( const exit_success& ) {}
   catch( ... ) {
      context.pop_frame();
      throw;
   }

   call_contract_result ret;
   ret.set_value( converter::to< std::string >( context.pop_frame().call_return ) );
   return ret;
}

THUNK_DEFINE_VOID( get_entry_point_result, get_entry_point )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   get_entry_point_result ret;
   ret.set_value( context.get_contract_entry_point() );
   return ret;
}

THUNK_DEFINE_VOID( get_contract_arguments_size_result, get_contract_arguments_size )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   get_contract_arguments_size_result ret;
   ret.set_value( (uint32_t)context.get_contract_call_args().size() );
   return ret;
}

THUNK_DEFINE_VOID( get_contract_arguments_result, get_contract_arguments )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   get_contract_arguments_result ret;
   ret.set_value( converter::as< std::string >( context.get_contract_call_args() ) );
   return ret;
}

THUNK_DEFINE( void, set_contract_result, ((const std::string&) ret) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   context.set_contract_return( converter::to< std::vector< std::byte > >( ret ) );
}

THUNK_DEFINE_VOID( get_head_info_result, get_head_info )
{
   context.resource_meter().use_compute_bandwidth( compute_load::medium );

   auto head = context.get_state_node();

   chain::head_info hi;
   hi.mutable_head_topology()->set_id( converter::as< std::string >( head->id() ) );
   hi.mutable_head_topology()->set_previous( converter::as< std::string >( head->parent_id() ) );
   hi.mutable_head_topology()->set_height( head->revision() );
   hi.set_last_irreversible_block( get_last_irreversible_block( context ).value() );

   if ( const auto* block = context.get_block(); block != nullptr )
   {
      hi.set_head_block_time( block->header().timestamp() );
   }
   else
   {
      auto val = system_call::get_object( context, database::space::kernel, database::key::head_block_time ).value();
      uint64_t time = val.size() > 0 ? converter::to< uint64_t >( val ) : 0;
      hi.set_head_block_time( time );
   }

   get_head_info_result ret;
   auto* v = ret.mutable_value();
   *v = hi;
   return ret;
}

THUNK_DEFINE( hash_result, hash, ((uint64_t) id, (const std::string&) obj, (uint64_t) size) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   auto multicodec = static_cast< crypto::multicodec >( id );
   switch ( multicodec )
   {
      case crypto::multicodec::sha1:
         [[fallthrough]];
      case crypto::multicodec::sha2_256:
         [[fallthrough]];
      case crypto::multicodec::sha2_512:
         [[fallthrough]];
      case crypto::multicodec::ripemd_160:
         break;
      default:
         KOINOS_THROW( unknown_hash_code, "unknown_hash_code" );
   }
   auto hash = crypto::hash( multicodec, obj, crypto::digest_size( size ) );

   hash_result ret;
   ret.set_value( converter::as< std::string >( hash ) );
   return ret;
}

THUNK_DEFINE( recover_public_key_result, recover_public_key, ((const std::string&) signature_data, (const std::string&) digest) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   KOINOS_ASSERT( signature_data.size() == 65, invalid_signature, "unexpected signature length" );
   crypto::recoverable_signature signature = converter::as< crypto::recoverable_signature >( signature_data );

   KOINOS_ASSERT( crypto::public_key::is_canonical( signature ), invalid_signature, "signature must be canonical" );

   auto pub_key = crypto::public_key::recover( signature, converter::to< crypto::multihash >( digest ) );
   KOINOS_ASSERT( pub_key.valid(), invalid_signature, "public key is invalid" );

   auto address = pub_key.to_address_bytes();
   recover_public_key_result ret;
   ret.set_value( address );
   return ret;
}

THUNK_DEFINE( get_transaction_payer_result, get_transaction_payer, ((const protocol::transaction&) transaction) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   std::string account = system_call::recover_public_key( context, transaction.signature_data(), converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, transaction.active() ) ) ).value();

   LOG(debug) << "(get_transaction_payer) transaction: " << transaction;

   get_transaction_payer_result ret;
   ret.set_value( account );
   return ret;
}

THUNK_DEFINE( get_transaction_rc_limit_result, get_transaction_rc_limit, ((const protocol::transaction&) transaction) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   protocol::active_transaction_data active_data;
   active_data.ParseFromString( transaction.active() );

   get_transaction_rc_limit_result ret;
   ret.set_value( active_data.rc_limit() );
   return ret;
}

THUNK_DEFINE_VOID( get_last_irreversible_block_result, get_last_irreversible_block )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   auto head = context.get_state_node();

   get_last_irreversible_block_result ret;
   ret.set_value( head->revision() > default_irreversible_threshold ? head->revision() - default_irreversible_threshold : 0 );

   return ret;
}

THUNK_DEFINE_VOID( get_caller_result, get_caller )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   get_caller_result ret;
   auto frame0 = context.pop_frame(); // get_caller frame
   auto frame1 = context.pop_frame(); // contract frame
   ret.set_caller( converter::as< std::string >( context.get_caller() ) );
   ret.set_caller_privilege( context.get_caller_privilege() );
   context.push_frame( std::move( frame1 ) );
   context.push_frame( std::move( frame0 ) );
   return ret;
}

THUNK_DEFINE_VOID( get_transaction_signature_result, get_transaction_signature )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   get_transaction_signature_result ret;
   ret.set_value( context.get_transaction().signature_data() );
   return ret;
}

THUNK_DEFINE( void, require_authority, ((const std::string&) account) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   auto digest = crypto::hash( crypto::multicodec::sha2_256, context.get_transaction().active() );
   std::string sig_account = system_call::recover_public_key( context, get_transaction_signature( context ).value(), converter::as< std::string >( digest ) ).value();

   KOINOS_ASSERT(
      account == sig_account,
      invalid_signature,
      "signature does not match", ("account", account)("sig_account", sig_account)
   );
}

THUNK_DEFINE_VOID( get_contract_id_result, get_contract_id )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   get_contract_id_result ret;
   ret.set_value( converter::as< std::string >( context.get_caller() ) );
   return ret;
}

THUNK_DEFINE( get_account_nonce_result, get_account_nonce, ((const std::string&) account ) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   auto obj = system_call::get_object( context, database::space::kernel, database::key::transaction_nonce( account ) ).value();

   get_account_nonce_result ret;

   if ( obj.size() > 0 )
      ret.set_value( converter::to< uint64_t >( obj ) );
   else
      ret.set_value( 0 );

   return ret;
}

THUNK_DEFINE( get_account_rc_result, get_account_rc, ((const std::string&) account) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   uint64_t max_resources = 10'000'000;
   get_account_rc_result ret;
   ret.set_value( max_resources );
   return ret;
}

THUNK_DEFINE( consume_account_rc_result, consume_account_rc, ((const std::string&) account, (uint64_t) rc) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   consume_account_rc_result ret;
   ret.set_value( true );
   return ret;
}

THUNK_DEFINE_VOID( get_resource_limits_result, get_resource_limits )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   resource_limit_data rd;

   rd.set_disk_storage_cost( 10 );
   rd.set_disk_storage_limit( 102'400 );

   rd.set_network_bandwidth_cost( 5 );
   rd.set_network_bandwidth_limit( 1'048'576 );

   rd.set_compute_bandwidth_cost( 1 );
   rd.set_compute_bandwidth_limit( 100'000'000 );

   get_resource_limits_result ret;
   *ret.mutable_value() = rd;
   return ret;
}

THUNK_DEFINE( consume_block_resources_result, consume_block_resources, ((uint64_t) disk, (uint64_t) network, (uint64_t) compute) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   LOG(info) << "Consumed disk storage: " << disk;
   LOG(info) << "Consumed network bandwidth: " << network;
   LOG(info) << "Consumed compute bandwidth: " << compute;

   consume_block_resources_result ret;
   ret.set_value( true );
   return ret;
}

THUNK_DEFINE_END();

} // koinos::chain
