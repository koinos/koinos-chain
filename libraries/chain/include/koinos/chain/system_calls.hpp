#pragma once
#include <cstdint>
#include <optional>
#include <map>
#include <vector>
#include <cstring>

#include <compiler_builtins.hpp>
#include <koinos/exception.hpp>
#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/chain/system_call_utils.hpp>
#include <koinos/chain/wasm/common.hpp>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/pack/classes.hpp>
#include <koinos/statedb/statedb.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-variable"

#ifndef __clang__
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <eosio/vm/backend.hpp>
#pragma GCC diagnostic pop
#include <eosio/vm/error_codes.hpp>
#include <eosio/vm/host_function.hpp>
#include <eosio/vm/exceptions.hpp>
#include <koinos/chain/wasm/type_conversion.hpp>

namespace koinos::chain {

DECLARE_KOINOS_EXCEPTION( system_call_not_overridable );

using wasm_allocator_type = eosio::vm::wasm_allocator;
using backend_type        = eosio::vm::backend< apply_context >;
using registrar_type      = eosio::vm::registered_host_functions< apply_context >;
using wasm_code_ptr       = eosio::vm::wasm_code_ptr;

using vl_blob             = koinos::protocol::vl_blob;

// When defining a new system call, we have essentially two different implementations.
// One of the implementations is considered upgradeable and can be overridden with
// VM code. The other implementation is prefixed with `internal_` and cannot be
// overridden and is the default implement if no override is provided.

// Use the macro SYSTEM_CALL_DECLARE to simultaneously declare both public and private versions.

struct system_api final
{
   system_api( apply_context& ctx );
   apply_context& context;

   void invoke_thunk( uint32_t tid, array_ptr< char > ret_ptr, uint32_t ret_len, array_ptr< const char > arg_ptr, uint32_t arg_len );
   void invoke_xcall( uint32_t xid, array_ptr< char > ret_ptr, uint32_t ret_len, array_ptr< const char > arg_ptr, uint32_t arg_len );
};

SYSTEM_CALL_DECLARE( void, prints, null_terminated_ptr str );

SYSTEM_CALL_DECLARE( bool, verify_block_header, const crypto::recoverable_signature& sig, const crypto::multihash_type& digest );

SYSTEM_CALL_DECLARE( void, apply_block, const protocol::active_block_data& b );
SYSTEM_CALL_DECLARE( void, apply_transaction, const protocol::transaction_type& t );
SYSTEM_CALL_DECLARE( void, apply_upload_contract_operation, const protocol::create_system_contract_operation& o );
SYSTEM_CALL_DECLARE( void, apply_execute_contract_operation, const protocol::contract_call_operation& op );

SYSTEM_CALL_DECLARE( bool, db_put_object, const statedb::object_space& space, const statedb::object_key& key, const vl_blob& obj );
SYSTEM_CALL_DECLARE( vl_blob, db_get_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );
SYSTEM_CALL_DECLARE( vl_blob, db_get_next_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );
SYSTEM_CALL_DECLARE( vl_blob, db_get_prev_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );

enum thunk_ids
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

// For any given system call, two slots are used. The first definition
// is considered overridable. The second system call slot is prefixed
// with `internal_` to denote a private unoverridable implementation.

// When adding a system call slot, use the provided macro SYSTEM_CALL_SLOTS
// to declare both a public and private implementation.

inline void register_host_functions()
{
   registrar_type::add< system_api, &system_api::invoke_thunk, wasm_allocator_type >( "env", "invoke_thunk" );
   registrar_type::add< system_api, &system_api::invoke_xcall, wasm_allocator_type >( "env", "invoke_xcall" );
}

} // koinos::chain
