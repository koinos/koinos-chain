#pragma once

#include <koinos/chain/thunk_utils.hpp>
#include <koinos/chain/types.hpp>

#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>

#include <koinos/chain/chain.pb.h>
#include <koinos/protocol/protocol.pb.h>
#include <koinos/protocol/system_call_ids.pb.h>

#include <koinos/state_db/state_db.hpp>

#define KOINOS_EXIT_SUCCESS 0
#define KOINOS_EXIT_FAILURE 1

namespace koinos::chain {

class execution_context;
class thunk_dispatcher;

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

THUNK_DECLARE( verify_block_signature_result, verify_block_signature, const std::string& digest, const std::string& active_data, const std::string& signature_data );
THUNK_DECLARE( verify_merkle_root_result, verify_merkle_root, const std::string& root, const std::vector< std::string >& hashes );

THUNK_DECLARE( void, apply_block, const protocol::block& block, bool check_passive_data, bool check_block_signature, bool check_transaction_signatures );
THUNK_DECLARE( void, apply_transaction, const protocol::transaction& trx );
THUNK_DECLARE( void, apply_upload_contract_operation, const protocol::upload_contract_operation& op );
THUNK_DECLARE( void, apply_call_contract_operation, const protocol::call_contract_operation& op );
THUNK_DECLARE( void, apply_set_system_call_operation, const protocol::set_system_call_operation& op );

THUNK_DECLARE( put_object_result, put_object, const object_space& space, const std::string& key, const std::string& obj );
THUNK_DECLARE( get_object_result, get_object, const object_space& space, const std::string& key, uint32_t object_size_hint = 0 );
THUNK_DECLARE( get_next_object_result, get_next_object, const object_space& space, const std::string& key, uint32_t object_size_hint = 0 );
THUNK_DECLARE( get_prev_object_result, get_prev_object, const object_space& space, const std::string& key, uint32_t object_size_hint = 0 );

THUNK_DECLARE( call_contract_result, call_contract, const std::string& contract_id, uint32_t entry_point, const std::string& args );

THUNK_DECLARE_VOID( get_entry_point_result, get_entry_point );
THUNK_DECLARE_VOID( get_contract_arguments_size_result, get_contract_arguments_size );
THUNK_DECLARE_VOID( get_contract_arguments_result, get_contract_arguments );

THUNK_DECLARE( void, set_contract_result, const std::string& ret );

THUNK_DECLARE_VOID( get_head_info_result, get_head_info );

THUNK_DECLARE( hash_result, hash, uint64_t code, const std::string& obj, uint64_t size = 0 );
THUNK_DECLARE( recover_public_key_result, recover_public_key, const std::string& signature_data, const std::string& digest );

THUNK_DECLARE( get_transaction_payer_result, get_transaction_payer, const protocol::transaction& tx );
THUNK_DECLARE( get_transaction_rc_limit_result, get_transaction_rc_limit, const protocol::transaction& tx );

THUNK_DECLARE_VOID( get_last_irreversible_block_result, get_last_irreversible_block );

THUNK_DECLARE_VOID( get_caller_result, get_caller );
THUNK_DECLARE_VOID( get_transaction_signature_result, get_transaction_signature );
THUNK_DECLARE( void, require_authority, const std::string& account );

THUNK_DECLARE_VOID( get_contract_id_result, get_contract_id );

THUNK_DECLARE( get_account_nonce_result, get_account_nonce, const std::string& account );

THUNK_DECLARE( get_account_rc_result, get_account_rc, const std::string& account );
THUNK_DECLARE( consume_account_rc_result, consume_account_rc, const std::string& account, uint64_t rc );

THUNK_DECLARE_VOID( get_resource_limits_result, get_resource_limits );
THUNK_DECLARE( consume_block_resources_result, consume_block_resources, uint64_t disk, uint64_t network, uint64_t compute );

THUNK_DECLARE( void, event, const std::string& name, const std::string& data, const std::vector< std::string >& impacted );

} // koinos::chain
