#pragma once

#include <koinos/chain/system_call_utils.hpp>

#include <koinos/chain/types.hpp>
#include <koinos/chain/wasm/common.hpp>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/pack/classes.hpp>
#include <koinos/statedb/statedb.hpp>

namespace koinos::chain {
class apply_context;

enum thunk_ids : uint32_t
{
   prints_thunk_id,
   verify_block_header_thunk_id,
   apply_block_thunk_id,
   apply_transaction_thunk_id,
   apply_upload_contract_operation_thunk_id,
   apply_execute_contract_operation_thunk_id,
   db_put_object_thunk_id,
   db_get_object_thunk_id,
   db_get_next_object_thunk_id,
   db_get_prev_object_thunk_id
};

namespace thunk {

/*
 * When defining a new thunk, we have essentially two different implementations.
 * One of the implementations is considered upgradeable and can be overridden with
 * VM code. The other implementation is suffixed with `_thunk` and cannot be
 * overridden and is the default implement if no override is provided.
 *
 * The internal version is called a thunk, and the external version is called a system
 * call. Thunks cannot be overridden are bound at compile time. System calls are bound
 * in bound via VM code and can be changed.
 *
 * Use the macro THUNK_DECLARE to simultaneously declare both the thunk and syscall.
 * In thunks.cpp using THUNK_DEFINE to define the thunk implementation. The syscall
 * implementation is automatically generated to respect the syscall override.
 *
 * When calling a thunk natively, you can call either version but the choice is important.
 * Calling `_thunk` will mean the called function will never change. This should be
 * reserved for low level, necessary implementations (e.g. I/O). Nearly every other
 * native call should be done to the syscall version (no suffix) so that if the call
 * needs to be upgraded later, the thunk will respect the override and properly call
 * to it.
 *
 * Some thunks may not need a syscall override (e.g. fixing a buggy thunk). In that
 * case, you can skip the THUNK_DECLARE/DEFINE macros and declare the thunk as a normal
 * function, remembering to register it in REGISTER_THUNKS along with defining a new,
 * unique, thunk_id.
 */

void hello( apply_context& ctx, hello_thunk_ret& ret, const hello_thunk_args& arg );

SYSTEM_CALL_DECLARE( void, prints, const std::string& str );

SYSTEM_CALL_DECLARE( bool, verify_block_header, const crypto::recoverable_signature& sig, const crypto::multihash_type& digest );

SYSTEM_CALL_DECLARE( void, apply_block, const protocol::active_block_data& b );
SYSTEM_CALL_DECLARE( void, apply_transaction, const protocol::transaction_type& t );
SYSTEM_CALL_DECLARE( void, apply_upload_contract_operation, const protocol::create_system_contract_operation& o );
SYSTEM_CALL_DECLARE( void, apply_execute_contract_operation, const protocol::contract_call_operation& op );

SYSTEM_CALL_DECLARE( bool, db_put_object, const statedb::object_space& space, const statedb::object_key& key, const vl_blob& obj );
SYSTEM_CALL_DECLARE( vl_blob, db_get_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );
SYSTEM_CALL_DECLARE( vl_blob, db_get_next_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );
SYSTEM_CALL_DECLARE( vl_blob, db_get_prev_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );

} } // koinos::chain::thunk
