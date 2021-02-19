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

namespace thunk {

KOINOS_DECLARE_EXCEPTION( exit_success );
KOINOS_DECLARE_EXCEPTION( exit_failure );
KOINOS_DECLARE_EXCEPTION( unknown_exit_code );
KOINOS_DECLARE_EXCEPTION( unknown_hash_code );
KOINOS_DECLARE_EXCEPTION( empty_block_header );
KOINOS_DECLARE_EXCEPTION( transaction_root_mismatch );
KOINOS_DECLARE_EXCEPTION( passive_root_mismatch );
KOINOS_DECLARE_EXCEPTION( invalid_block_signature );

/*
 * When defining a new thunk, we have essentially two different implementations.
 * One of the implementations is considered upgradeable and can be overridden with
 * VM code. The other implementation is suffixed with `_thunk` and cannot be
 * overridden and is the default implement if no override is provided.
 *
 * The internal version is called a thunk, and the external version is called a system
 * call. Thunks are bound at compile time and cannot be overridden. System calls are
 * bound via VM code and can be changed.
 *
 * Use the macro THUNK_DECLARE to simultaneously declare both the thunk and syscall.
 * In thunks.cpp use THUNK_DEFINE to define the thunk implementation. The syscall
 * implementation is automatically generated to respect the syscall override.
 *
 * When calling a thunk natively, you can call either version but the choice is important.
 * Calling `_thunk` will mean the called function will never change. This should be
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

THUNK_DECLARE( bool, verify_block_signature, const variable_blob& signature_data, const multihash& digest );
THUNK_DECLARE( bool, verify_merkle_root, const multihash& root, const std::vector< multihash >& hashes );

THUNK_DECLARE( void, apply_block,
   const protocol::block& block,
   boolean enable_check_passive_data,
   boolean enable_check_block_signature,
   boolean enable_check_transaction_signatures );
THUNK_DECLARE( void, apply_transaction, const opaque< protocol::transaction >& trx );
THUNK_DECLARE( void, apply_reserved_operation, const protocol::reserved_operation& o );
THUNK_DECLARE( void, apply_upload_contract_operation, const protocol::create_system_contract_operation& o );
THUNK_DECLARE( void, apply_execute_contract_operation, const protocol::contract_call_operation& op );
THUNK_DECLARE( void, apply_set_system_call_operation, const protocol::set_system_call_operation& op );

THUNK_DECLARE( bool, db_put_object, const statedb::object_space& space, const statedb::object_key& key, const variable_blob& obj );
THUNK_DECLARE( variable_blob, db_get_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );
THUNK_DECLARE( variable_blob, db_get_next_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );
THUNK_DECLARE( variable_blob, db_get_prev_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );

THUNK_DECLARE( variable_blob, execute_contract, const contract_id_type& contract_id, uint32_t entry_point, const variable_blob& args);

THUNK_DECLARE_VOID( uint32_t, get_contract_args_size );
THUNK_DECLARE_VOID( variable_blob, get_contract_args );

THUNK_DECLARE( void, set_contract_return, const variable_blob& ret );

THUNK_DECLARE_VOID( chain::head_info, get_head_info );

THUNK_DECLARE( multihash, hash, uint64_t code, const variable_blob& obj, uint64_t size = 0 );

} } // koinos::chain::thunk
