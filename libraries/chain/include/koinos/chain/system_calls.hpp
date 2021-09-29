#pragma once

#include <koinos/chain/thunk_utils.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/chain/wasm/common.hpp>

#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>

#include <koinos/chain/chain.pb.h>
#include <koinos/protocol/protocol.pb.h>
#include <koinos/protocol/system_call_ids.pb.h>

#include <koinos/state_db/state_db.hpp>

#define KOINOS_EXIT_SUCCESS 0
#define KOINOS_EXIT_FAILURE 1

#define KOINOS_MAX_METER_TICKS (10 * int64_t(1000) * int64_t(1000))

namespace koinos::chain {

class apply_context;
class thunk_dispatcher;

// std::optional< thunk_id > get_default_system_call_entry( system_call_id sid );
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
THUNK_DECLARE( void, exit_contract, uint32_t exit_code );

THUNK_DECLARE( verify_block_signature_return, verify_block_signature, const std::string& digest, const std::string& active_data, const std::string& signature_data );
THUNK_DECLARE( verify_merkle_root_return, verify_merkle_root, const std::string& root, const std::vector< std::string >& hashes );

THUNK_DECLARE( void, apply_block, const protocol::block& block, bool check_passive_data, bool check_block_signature, bool check_transaction_signatures );
THUNK_DECLARE( void, apply_transaction, const protocol::transaction& trx );
THUNK_DECLARE( void, apply_upload_contract_operation, const protocol::upload_contract_operation& op );
THUNK_DECLARE( void, apply_call_contract_operation, const protocol::call_contract_operation& op );
THUNK_DECLARE( void, apply_set_system_call_operation, const protocol::set_system_call_operation& op );

THUNK_DECLARE( put_object_return, put_object, const std::string& space, const std::string& key, const std::string& obj );
THUNK_DECLARE( get_object_return, get_object, const std::string& space, const std::string& key, uint32_t object_size_hint = 0 );
THUNK_DECLARE( get_next_object_return, get_next_object, const std::string& space, const std::string& key, uint32_t object_size_hint = 0 );
THUNK_DECLARE( get_prev_object_return, get_prev_object, const std::string& space, const std::string& key, uint32_t object_size_hint = 0 );

THUNK_DECLARE( call_contract_return, call_contract, const std::string& contract_id, uint32_t entry_point, const std::string& args );

THUNK_DECLARE_VOID( get_entry_point_return, get_entry_point );
THUNK_DECLARE_VOID( get_contract_args_size_return, get_contract_args_size );
THUNK_DECLARE_VOID( get_contract_args_return, get_contract_args );

THUNK_DECLARE( void, set_contract_return, const std::string& ret );

THUNK_DECLARE_VOID( get_head_info_return, get_head_info );

THUNK_DECLARE( hash_return, hash, uint64_t code, const std::string& obj, uint64_t size = 0 );
THUNK_DECLARE( recover_public_key_return, recover_public_key, const std::string& signature_data, const std::string& digest );

THUNK_DECLARE( get_transaction_payer_return, get_transaction_payer, const protocol::transaction& tx );
THUNK_DECLARE( get_max_account_resources_return, get_max_account_resources, const std::string& account );
THUNK_DECLARE( get_transaction_resource_limit_return, get_transaction_resource_limit, const protocol::transaction& tx);

THUNK_DECLARE_VOID( get_last_irreversible_block_return, get_last_irreversible_block );

THUNK_DECLARE_VOID( get_caller_return, get_caller );
THUNK_DECLARE_VOID( get_transaction_signature_return, get_transaction_signature );
THUNK_DECLARE( void, require_authority, const std::string& account );

THUNK_DECLARE_VOID( get_contract_id_return, get_contract_id );

THUNK_DECLARE( get_account_nonce_return, get_account_nonce, const std::string& account );

} // koinos::chain
