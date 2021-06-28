#pragma once

#include <koinos/chain/thunk_utils.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/chain/wasm/common.hpp>

#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>

#include <koinos/pack/classes.hpp>

#include <koinos/statedb/statedb.hpp>

#define KOINOS_EXIT_SUCCESS 0
#define KOINOS_EXIT_FAILURE 1

namespace koinos::chain {

class apply_context;
class thunk_dispatcher;

std::optional< thunk_id > get_default_system_call_entry( system_call_id sid );
void register_thunks( thunk_dispatcher& td );

/*
 * When defining a new thunk, we have essentially two different implementations.
 * One of the implementations is considered upgradeable and can be overridden with
 * VM code. The other implementation resides in the 'thunk' namespace and cannot be
 * overridden and is the default implement if no override is provided.
 *
 * The internal version is called a thunk, and the external version is called a system
 * call. Thunks are bound at compile time and cannot be overridden. System calls are
 * bound via VM code and can be changed.
 *
 * Use the macro THUNK_DECLARE to simultaneously declare both the thunk and syscall.
 * In system_calls.cpp use THUNK_DEFINE to define the thunk implementation. The syscall
 * implementation is automatically generated to respect the syscall override.
 *
 * When calling a thunk natively, you can call either version but the choice is important.
 * Calling a thunk will mean the called function will never change. This should be
 * reserved for low level, necessary implementations (e.g. I/O). Nearly every other
 * native call should be done to the syscall version (no suffix) so that if the call
 * needs to be upgraded later, the syscall will respect the override and properly call
 * to it.
 *
 * Some thunks may not need a syscall override (e.g. fixing a buggy thunk). In that
 * case, you can skip the THUNK_DECLARE/DEFINE macros and declare the thunk as a normal
 * function, remembering to register it in REGISTER_THUNKS along with defining a new,
 * unique, thunk_id.
 */

THUNK_DECLARE( void, prints, const std::string& str );
THUNK_DECLARE( void, exit_contract, uint8_t exit_code );

THUNK_DECLARE( bool, verify_block_signature, const multihash& digest, const opaque< protocol::active_block_data >& active_data, const variable_blob& signature_data );
THUNK_DECLARE( bool, verify_merkle_root, const multihash& root, const std::vector< multihash >& hashes );

THUNK_DECLARE( void, apply_block,
   const protocol::block& block,
   boolean check_passive_data,
   boolean check_block_signature,
   boolean check_transaction_signatures );
THUNK_DECLARE( void, apply_transaction, const protocol::transaction& trx );
THUNK_DECLARE( void, apply_reserved_operation, const protocol::reserved_operation& o );
THUNK_DECLARE( void, apply_upload_contract_operation, const protocol::create_system_contract_operation& o );
THUNK_DECLARE( void, apply_execute_contract_operation, const protocol::call_contract_operation& op );
THUNK_DECLARE( void, apply_set_system_call_operation, const protocol::set_system_call_operation& op );

THUNK_DECLARE( bool, db_put_object, const statedb::object_space& space, const statedb::object_key& key, const variable_blob& obj );
THUNK_DECLARE( variable_blob, db_get_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );
THUNK_DECLARE( variable_blob, db_get_next_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );
THUNK_DECLARE( variable_blob, db_get_prev_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );

THUNK_DECLARE( variable_blob, execute_contract, const contract_id_type& contract_id, uint32_t entry_point, const variable_blob& args );

THUNK_DECLARE_VOID( uint32_t, get_entry_point );
THUNK_DECLARE_VOID( uint32_t, get_contract_args_size );
THUNK_DECLARE_VOID( variable_blob, get_contract_args );

THUNK_DECLARE( void, set_contract_return, const variable_blob& ret );

THUNK_DECLARE_VOID( chain::head_info, get_head_info );

THUNK_DECLARE( multihash, hash, uint64_t code, const variable_blob& obj, uint64_t size = 0 );
THUNK_DECLARE( variable_blob, recover_public_key, const variable_blob& signature_data, const multihash& digest );

THUNK_DECLARE( protocol::account_type, get_transaction_payer, const protocol::transaction& tx );
THUNK_DECLARE( uint128, get_max_account_resources, const protocol::account_type& account );
THUNK_DECLARE( uint128, get_transaction_resource_limit, const protocol::transaction& tx);

THUNK_DECLARE_VOID( block_height_type, get_last_irreversible_block );

THUNK_DECLARE_VOID( get_caller_return, get_caller );
THUNK_DECLARE_VOID( variable_blob, get_transaction_signature );
THUNK_DECLARE( void, require_authority, const protocol::account_type& );

THUNK_DECLARE_VOID( contract_id_type, get_contract_id );
THUNK_DECLARE_VOID( timestamp_type, get_head_block_time );

THUNK_DECLARE( get_account_nonce_return, get_account_nonce, const protocol::account_type& account );

} // koinos::chain
