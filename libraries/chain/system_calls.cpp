#include <algorithm>
#include <string>

#include <google/protobuf/message.h>

#include <koinos/bigint.hpp>
#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/constants.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/log.hpp>

#define KOINOS_MAX_TICKS_PER_BLOCK (100 * int64_t(1000) * int64_t(1000))

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

      (db_put_object)
      (db_get_object)
      (db_get_next_object)
      (db_get_prev_object)

      (call_contract)

      (get_entry_point)
      (get_contract_args_size)
      (get_contract_args)
      (set_contract_return)

      (get_head_info)
      (hash)
      (recover_public_key)

      (get_transaction_payer)
      (get_max_account_resources)
      (get_transaction_resource_limit)

      (get_last_irreversible_block)

      (get_caller)
      (get_transaction_signature)
      (require_authority)

      (get_contract_id)

      (get_account_nonce)
   )
}

// TODO: Should this be a thunk?
bool is_system_space( const statedb::object_space& space_id )
{
   return space_id == database::space::contract ||
          space_id == database::space::system_call_dispatch ||
          space_id == database::space::kernel;
}

THUNK_DEFINE_BEGIN();

THUNK_DEFINE( void, prints, ((const std::string&) str) )
{
   context.console_append( str );
}

THUNK_DEFINE( void, exit_contract, ((uint8_t) exit_code) )
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

THUNK_DEFINE( verify_block_signature_return, verify_block_signature, ((const std::string&) id, (const std::string&) active_data, (const std::string&) signature_data) )
{
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
         auto obj = system_call::db_get_object( context, database::space::kernel, database::key::chain_id );
         chain_id = converter::to< crypto::multihash >( obj );
      }
   );

   verify_block_signature_return ret;
   ret.set_value( chain_id == crypto::hash( crypto::multicodec::sha2_256, crypto::public_key::recover( sig, block_id ).to_address() ) );
   return ret;
}

THUNK_DEFINE( verify_merkle_root_return, verify_merkle_root, ((const std::string&) root, (const std::vector< std::string >&) hashes) )
{
   std::vector< crypto::multihash > leaves;

   leaves.resize( hashes.size() );
   std::transform( std::begin( hashes ), std::end( hashes ), std::begin( leaves ), []( const std::string& s ) { return converter::to< crypto::multihash >( s ); } );

   auto root_hash = converter::to< crypto::multihash >( root );
   auto mtree = crypto::merkle_tree( root_hash.code(), hashes );

   verify_merkle_root_return ret;
   ret.set_value( mtree.root()->hash() == root_hash );
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
   KOINOS_TODO( "Should this be a more specific exception?" );
   KOINOS_ASSERT( block.id().size(), koinos::exception, "missing expected field in block: ${f}", ("f", "id") );
   KOINOS_ASSERT( block.has_header(), koinos::exception, "missing expected field in block: ${f}", ("f", "header") );
   KOINOS_ASSERT( block.header().previous().size(), koinos::exception, "missing expected field in block_header: ${f}", ("f", "previous") );
   KOINOS_ASSERT( block.header().height(), koinos::exception, "missing expected field in block_header: ${f}", ("f", "height") );
   KOINOS_ASSERT( block.header().timestamp(), koinos::exception, "missing expected field in block_header: ${f}", ("f", "timestamp") );
   KOINOS_ASSERT( block.active().size(), koinos::exception, "missing expected field: ${f}", ("f", "active") );
   KOINOS_ASSERT( block.passive().size(), koinos::exception, "missing expected field: ${f}", ("f", "passive") );
   KOINOS_ASSERT( block.signature_data().size(), koinos::exception, "missing expected field: ${f}", ("f", "signature_data") );

   protocol::active_block_data active_data;
   active_data.ParseFromString( block.active() );

   const crypto::multihash tx_root = converter::to< crypto::multihash >( active_data.transaction_merkle_root() );
   size_t tx_count = block.transactions_size();

   // Check transaction Merkle root
   std::vector< std::string > hashes( tx_count );

   for ( const auto& trx : block.transactions() )
   {
      hashes.emplace_back( converter::as< std::string >( crypto::hash( tx_root.code(), trx ) ) );
   }

//   for ( std::size_t i = 0; i < tx_count; i++ )
//   {
//      hashes[i] = converter::as< std::string >( crypto::hash( tx_root.code(), block.transactions( i ).active() ) );
//   }

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

   system_call::db_put_object( context, database::space::kernel, database::key::head_block_time, converter::as< std::string >( block.header().timestamp() ) );

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
      std::vector< std::string > passives( 2 * ( block.transactions().size() + 1 ) );

      passives.emplace_back( converter::as< std::string >( crypto::hash( passive_root.code(), block.passive() ) ) );
      passives.emplace_back( converter::as< std::string >( crypto::multihash::empty( passive_root.code() ) ) );

      for ( const auto& trx : block.transactions() )
      {
         passives.emplace_back( converter::as< std::string >( crypto::hash( passive_root.code(), trx.passive() ) ) );
         passives.emplace_back( converter::as< std::string >( crypto::hash( passive_root.code(), trx.signature_data() ) ) );
      }

      KOINOS_ASSERT( system_call::verify_merkle_root( context, active_data.passive_data_merkle_root(), hashes ).value(), passive_root_mismatch, "passive merkle root does not match" );
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

   auto block_node = context.get_state_node();
   int64_t ticks_used = 0;

   for( const auto& tx : block.transactions() )
   {
      auto trx_node = block_node->create_anonymous_node();
      context.set_state_node( trx_node );

      // At this point, ticks_used could potentially be very close to KOINOS_MAX_TICKS_PER_BLOCK.
      //
      // This means we might use up to one transaction's worth of ticks
      // in excess of KOINOS_MAX_TICKS_PER_BLOCK determining that the block uses too many ticks.
      //
      // We could potentially have the per-transaction set_meter_ticks() set a value that
      // causes a transaction to terminate as soon as the block limit is exceeded, but this
      // seems like this would add architectural complexity for the purpose of needlessly
      // optimizing an unusual case.

      try
      {
         system_call::apply_transaction( context, tx );
         trx_node->commit();
      }
      catch ( ... ) { /* Do nothing will result in trx reversion */ }

      ticks_used += context.get_used_meter_ticks();

      KOINOS_ASSERT( ticks_used <= KOINOS_MAX_TICKS_PER_BLOCK, tick_meter_exception, "per-block tick limit exceeded" );
   }
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
   auto payer_key = converter::to< uint160_t >( payer );
   auto trx_nonce_key = converter::as< uint64_t >( crypto::hash( crypto::multicodec::ripemd_160, std::string( "object_key::nonce" ) ).digest() );

   system_call::db_put_object( ctx, database::space::kernel, converter::as< statedb::object_key >( payer_key, trx_nonce_key ), converter::as< std::string >( nonce ) );
}

THUNK_DEFINE( void, apply_transaction, ((const protocol::transaction&) trx) )
{
   KOINOS_ASSERT( !context.is_in_user_code(), insufficient_privileges, "calling privileged thunk from non-privileged code" );

   auto setter = transaction_setter( context, trx );

   protocol::active_transaction_data active_data;
   active_data.ParseFromString( trx.active() );

   auto payer = system_call::get_transaction_payer( context, trx ).value();
   system_call::require_authority( context, payer );
   require_payer_transaction_nonce( context, payer, active_data.nonce() );

   KOINOS_ASSERT( active_data.resource_limit() <= KOINOS_MAX_METER_TICKS, tick_max_too_high_exception, "tick max is too high" );
   context.set_meter_ticks( active_data.resource_limit() );

   for ( const auto& o : active_data.operations() )
   {
      if ( o.has_upload_contract_operation )
         system_call::apply_upload_contract_operation( context, op );
      else if ( o.has_call_contract_operation() )
         system_call::apply_call_contract_operation( context, op );
      else if ( o.has_set_system_call_operation() )
         system_call::apply_set_system_call_operation( context, op );
      else
         KOINOS_THROW( koinos::exception, "unknown operation" );
   }

   // Next nonce should be the current nonce + 1
   update_payer_transaction_nonce( context, payer, active_data.nonce() + 1 );

   LOG(debug) << "(apply_transaction) transaction " << trx.id() << " used ticks: " << context.get_used_meter_ticks();
}

THUNK_DEFINE( void, apply_upload_contract_operation, ((const protocol::upload_contract_operation&) o) )
{
   KOINOS_ASSERT( !context.is_in_user_code(), insufficient_privileges, "calling privileged thunk from non-privileged code" );

   protocol::active_transaction_data active_data;
   active_data.ParseFromString( context.get_transaction().active() );

   auto tx_id       = crypto::hash( crypto::multicodec::sha2_256, active_data );
   auto sig_account = system_call::recover_public_key( context, get_transaction_signature( context ), converter::as< std::string >( tx_id ) ).value();
   auto signer_hash = crypto::hash( crypto::multicodec:ripemd_160, sig_account );
   auto contract_id = converter::to< crypto::multihash >( o.contract_id() );

   KOINOS_ASSERT(
      signer_hash.digest().size() == contract_id.digest().size() && std::equal( signer_hash.digest().begin(), signer_hash.digest.end(), contract_id.digest().begin() ),
      invalid_signature,
      "signature does not match", ("contract_id", contract_id)("signer_hash", signer_hash)
   );

   system_call::db_put_object( context, database::contract_space, converter::as< statedb::object_key >( contract_id ), o.bytecode );
}

THUNK_DEFINE( void, apply_call_contract_operation, ((const protocol::call_contract_operation&) o) )
{
   KOINOS_ASSERT( !context.is_in_user_code(), insufficient_privileges, "calling privileged thunk from non-privileged code" );

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

   crypto::multihash chain_id;

   with_stack_frame(
      context,
      stack_frame {
         .call = crypto::hash( crypto::multicodec::ripemd_160, "retrieve_chain_id"s ).digest(),
         .call_privilege = privilege::kernel_mode,
      },
      [&]() {
         auto obj = system_call::db_get_object( context, database::kernel_space, database::key::chain_id ).value();
         chain_id = converter::to< crypto::multihash >( obj );
      }
   );

   const auto& tx = context.get_transaction();
   crypto::recoverable_signature sig;
   std::memcpy( sig.data(), tx.signature_data().c_str(), tx.signature_data().size() );

   KOINOS_ASSERT(
      chain_id == crypto::hash( crypto::multicodec::sha2_256, crypto::public_key::recover( sig, converter::to< crypto::multihash >( tx.id() ) ).to_address() ),
      insufficient_privileges,
      "transaction does not have the required authority to override system calls"
   );

   if ( o.target().has_contract_call_bundle() )
   {
      auto contract_id = converter::to< crypto::multihash >( scb.contract_id() );
      auto contract = db_get_object( context, database::contract_space, converter::to< statedb::object_key >( contract_id ) ).value();
      KOINOS_ASSERT( contract.size(), invalid_contract, "contract does not exist" );
      KOINOS_ASSERT( ( o.call_id() != ssystem_call_id::call_contract ), forbidden_override, "cannot override call_contract" );
   }
   else if ( o.target().thunk_id() )
   {
      KOINOS_ASSERT( thunk_dispatcher::instance().thunk_exists( tid ), thunk_not_found, "thunk ${tid} does not exist", ("tid", tid) );
   }
   else
   {
      KOINOS_THROW( unknown_system_call, "set_system_call invoked with unknown override" );
   }

   // Place the override in the database
   system_call::db_put_object( context, database::space::system_call_dispatch, o.call_id, pack::to_variable_blob( o.target ) );
}

void check_db_permissions( const apply_context& context, const statedb::object_space& space )
{
   auto privilege = context.get_privilege();
   auto caller = pack::from_variable_blob< uint160 >( context.get_caller() );
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

THUNK_DEFINE( db_put_object_return, db_put_object, ((const statedb::object_space&) space, (const statedb::object_key&) key, (const variable_blob&) obj) )
{
   KOINOS_ASSERT( !context.is_read_only(), read_only_context, "Cannot put object during read only call" );
   check_db_permissions( context, space );

   auto state = context.get_state_node();
   KOINOS_ASSERT( state, state_node_not_found, "Current state node does not exist" );
   statedb::put_object_args put_args;
   put_args.space = space;
   put_args.key = key;
   put_args.buf = obj.data();
   put_args.object_size = obj.size();

   statedb::put_object_result put_res;
   state->put_object( put_res, put_args );

   return put_res.object_existed;
}

THUNK_DEFINE( db_get_object_return, db_get_object, ((const statedb::object_space&) space, (const statedb::object_key&) key, (int32_t) object_size_hint) )
{
   check_db_permissions( context, space );

   auto state = context.get_state_node();
   KOINOS_ASSERT( state, state_node_not_found, "Current state node does not exist" );

   statedb::get_object_args get_args;
   get_args.space = space;
   get_args.key = key;
   get_args.buf_size = object_size_hint > 0 ? object_size_hint : STATE_DB_MAX_OBJECT_SIZE;

   variable_blob object_buffer;
   object_buffer.resize( get_args.buf_size );
   get_args.buf = object_buffer.data();

   statedb::get_object_result get_res;
   state->get_object( get_res, get_args );

   if( get_res.key == get_args.key && get_res.size > 0 )
      object_buffer.resize( get_res.size );
   else
      object_buffer.clear();

   return object_buffer;
}

THUNK_DEFINE( db_get_next_object_return, db_get_next_object, ((const statedb::object_space&) space, (const statedb::object_key&) key, (int32_t) object_size_hint) )
{
   check_db_permissions( context, space );

   auto state = context.get_state_node();
   KOINOS_ASSERT( state, state_node_not_found, "Current state node does not exist" );
   statedb::get_object_args get_args;
   get_args.space = space;
   get_args.key = key;
   get_args.buf_size = object_size_hint > 0 ? object_size_hint : STATE_DB_MAX_OBJECT_SIZE;

   variable_blob object_buffer;
   object_buffer.resize( get_args.buf_size );
   get_args.buf = object_buffer.data();

   statedb::get_object_result get_res;
   state->get_next_object( get_res, get_args );

   if( get_res.size > 0 )
      object_buffer.resize( get_res.size );
   else
      object_buffer.clear();

   return object_buffer;
}

THUNK_DEFINE( db_get_prev_object_return, db_get_prev_object, ((const statedb::object_space&) space, (const statedb::object_key&) key, (int32_t) object_size_hint) )
{
   check_db_permissions( context, space );

   auto state = context.get_state_node();
   KOINOS_ASSERT( state, state_node_not_found, "Current state node does not exist" );
   statedb::get_object_args get_args;
   get_args.space = space;
   get_args.key = key;
   get_args.buf_size = object_size_hint > 0 ? object_size_hint : STATE_DB_MAX_OBJECT_SIZE;

   variable_blob object_buffer;
   object_buffer.resize( get_args.buf_size );
   get_args.buf = object_buffer.data();

   statedb::get_object_result get_res;
   state->get_prev_object( get_res, get_args );

   if( get_res.size > 0 )
      object_buffer.resize( get_res.size );
   else
      object_buffer.clear();

   return object_buffer;
}

THUNK_DEFINE( call_contract_return, call_contract, ((const contract_id_type&) contract_id, (uint32_t) entry_point, (const variable_blob&) args) )
{
   uint256_t contract_key = pack::from_fixed_blob< uint160_t >( contract_id );

   // We need to be in kernel mode to read the contract data
   variable_blob bytecode;
   with_stack_frame(
      context,
      stack_frame {
         .call = crypto::hash( CRYPTO_RIPEMD160_ID, std::string( "call_contract" ) ).digest,
         .call_privilege = privilege::kernel_mode,
      },
      [&]()
      {
         bytecode = system_call::db_get_object( context, database::contract_space, contract_key );
         KOINOS_ASSERT( bytecode.size(), invalid_contract, "Contract does not exist" );
      }
   );

   wasm_allocator_type wa;
   wasm_code_ptr bytecode_ptr( (uint8_t*)bytecode.data(), bytecode.size() );
   backend_type backend( bytecode_ptr, bytecode_ptr.bounds(), registrar_type{} );

   backend.set_wasm_allocator( &wa );
   backend.initialize();

   context.push_frame( stack_frame {
      .call = pack::to_variable_blob( contract_id ),
      .call_privilege = context.get_privilege(),
      .call_args = args,
      .entry_point = entry_point
   } );

   try
   {
      backend( &context, "env", "_start" );
   }
   catch( const exit_success& ) {}
   catch( ... ) {
      context.pop_frame();
      wa.free();
      throw;
   }

   wa.free();
   return context.pop_frame().call_return;
}

THUNK_DEFINE_VOID( get_entry_point_return, get_entry_point )
{
   return context.get_contract_entry_point();
}

THUNK_DEFINE_VOID( get_contract_args_size, get_contract_args_size )
{
   return (uint32_t)context.get_contract_call_args().size();
}

THUNK_DEFINE_VOID( get_contract_args_return, get_contract_args )
{
   return context.get_contract_call_args();
}

THUNK_DEFINE( void, set_contract_return, ((const variable_blob&) ret) )
{
   context.set_contract_return( ret );
}

THUNK_DEFINE_VOID( get_head_info_return, get_head_info )
{
   auto head = context.get_state_node();
   const block_height_type IRREVERSIBLE_THRESHOLD = block_height_type{ 6 };

   chain::head_info hi;
   hi.head_topology.id       = head->id();
   hi.head_topology.previous = head->parent_id();
   hi.head_topology.height   = head->revision();
   hi.last_irreversible_height = get_last_irreversible_block( context );

   auto block_ptr = context.get_block();
   if ( block_ptr )
   {
      return block_ptr->header.timestamp;
   }

   auto key = database::key_from_string( database::key::head_block_time );
   return pack::from_variable_blob< timestamp_type >( db_get_object( context, database::kernel_space, key ) );

   return hi;
}

THUNK_DEFINE( hash_return, hash, ((uint64_t) id, (const variable_blob&) obj, (uint64_t) size) )
{
   KOINOS_ASSERT( crypto::multihash_id_is_known( id ), unknown_hash_code, "Unknown hash code" );
   auto hash = crypto::hash_str( id, obj.data(), obj.size(), size );
   return hash;
}

THUNK_DEFINE( recover_public_key_return, recover_public_key, ((const variable_blob&) signature_data, (const multihash&) digest) )
{
   KOINOS_ASSERT( signature_data.size() == 65, invalid_signature, "Unexpected signature length" );
   crypto::recoverable_signature signature;
   std::copy_n( signature_data.begin(), signature_data.size(), signature.begin() );

   KOINOS_ASSERT( crypto::public_key::is_canonical( signature ), invalid_signature, "Signature must be canonical" );

   auto pub_key = crypto::public_key::recover( signature, digest );
   KOINOS_ASSERT( pub_key.valid(), invalid_signature, "Public key is invalid" );

   auto address = pub_key.to_address();
   return variable_blob( address.begin(), address.end() );
}

THUNK_DEFINE( get_transaction_payer_return, get_transaction_payer, ((const protocol::transaction&) transaction) )
{
   multihash digest = crypto::hash( CRYPTO_SHA2_256_ID, transaction.active_data );
   protocol::account_type account = system_call::recover_public_key( context, transaction.signature_data, digest );

   LOG(debug) << "(get_transaction_payer) transaction: " << transaction;
   KOINOS_TODO( "stream override for variable_blob needs to be updated" );
   pack::json j;
   pack::to_json( j, account );
   LOG(debug) << "(get_transaction_payer) public_key: " << j.dump();

   return account;
}

THUNK_DEFINE( get_max_account_resources_return, get_max_account_resources, ((const protocol::account_type&) account) )
{
   uint128 max_resources = 10'000'000;
   return max_resources;
}

THUNK_DEFINE( get_transaction_resource_limit_return, get_transaction_resource_limit, ((const protocol::transaction&) transaction) )
{
   transaction.active_data.unbox();
   const auto& active_data = transaction.active_data.get_const_native();

   return active_data.resource_limit;
}

THUNK_DEFINE_VOID( get_last_irreversible_block_return, get_last_irreversible_block )
{
   auto head = context.get_state_node();
   return block_height_type( head->revision() > default_irreversible_threshold ? head->revision() - default_irreversible_threshold : 0 );
}

THUNK_DEFINE_VOID( get_caller_return, get_caller )
{
   get_caller_return ret;
   auto frame0 = context.pop_frame(); // get_caller frame
   auto frame1 = context.pop_frame(); // contract frame
   ret.caller = context.get_caller();
   ret.caller_privilege = context.get_caller_privilege();
   context.push_frame( std::move( frame1 ) );
   context.push_frame( std::move( frame0 ) );
   return ret;
}

THUNK_DEFINE_VOID( get_transaction_signature_return, get_transaction_signature )
{
   return context.get_transaction().signature_data;
}

THUNK_DEFINE( void, require_authority, ((const protocol::account_type&) account) )
{
   auto digest = crypto::hash( CRYPTO_SHA2_256_ID, context.get_transaction().active_data );
   protocol::account_type sig_account = system_call::recover_public_key( context, get_transaction_signature( context ), digest );
   KOINOS_ASSERT( sig_account.size() == account.size() &&
      std::equal(sig_account.begin(), sig_account.end(), account.begin()), invalid_signature, "signature does not match",
      ("account", account)("sig_account", sig_account) );
}

THUNK_DEFINE_VOID( get_contract_id_return, get_contract_id )
{
   return pack::from_variable_blob< contract_id_type >( context.get_caller() );
}

THUNK_DEFINE( get_account_nonce_return, get_account_nonce, ((const protocol::account_type&) account ) )
{
   variable_blob vkey;
   pack::to_variable_blob( vkey, account );
   pack::to_variable_blob( vkey, std::string{ database::key::transaction_nonce }, true );

   statedb::object_key key;
   key = pack::from_variable_blob< statedb::object_key >( vkey );
   auto obj = system_call::db_get_object( context, database::kernel_space, key );
   if ( obj.size() > 0 )
   {
      return { pack::from_variable_blob< uint64 >( obj ) };
   }

   return { 0 };
}

THUNK_DEFINE_END();

} // koinos::chain
