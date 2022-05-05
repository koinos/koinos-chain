#pragma once

#include <google/protobuf/struct.pb.h>

#include <koinos/chain/thunk_utils.hpp>
#include <koinos/chain/types.hpp>

#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>

#include <koinos/chain/chain.pb.h>
#include <koinos/chain/system_calls.pb.h>
#include <koinos/chain/system_call_ids.pb.h>
#include <koinos/protocol/protocol.pb.h>

#include <koinos/state_db/state_db.hpp>

namespace koinos::chain {

namespace constants {
   constexpr int32_t chain_success = 0;
   constexpr int32_t chain_reversion = 1;
   constexpr int32_t chain_failure = -1;
}

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

// General Blockchain Management

THUNK_DECLARE_VOID( get_head_info_result, get_head_info );
THUNK_DECLARE( void, apply_block, const protocol::block& block );
THUNK_DECLARE( void, apply_transaction, const protocol::transaction& trx );
THUNK_DECLARE( void, apply_upload_contract_operation, const protocol::upload_contract_operation& op );
THUNK_DECLARE( void, apply_call_contract_operation, const protocol::call_contract_operation& op );
THUNK_DECLARE( void, apply_set_system_call_operation, const protocol::set_system_call_operation& op );
THUNK_DECLARE( void, apply_set_system_contract_operation, const protocol::set_system_contract_operation& op );
THUNK_DECLARE_VOID( void, pre_block_callback );
THUNK_DECLARE_VOID( void, pre_transaction_callback );
THUNK_DECLARE_VOID( void, post_block_callback );
THUNK_DECLARE_VOID( void, post_transaction_callback );
THUNK_DECLARE_VOID( get_chain_id_result, get_chain_id );

// System Helpers

THUNK_DECLARE( process_block_signature_result, process_block_signature, const std::string& digest, const protocol::block_header& header, const std::string& signature_data );
THUNK_DECLARE_VOID( get_transaction_result, get_transaction );
THUNK_DECLARE( get_transaction_field_result, get_transaction_field, const std::string& field );
THUNK_DECLARE_VOID( get_block_result, get_block );
THUNK_DECLARE( get_block_field_result, get_block_field, const std::string& field );
THUNK_DECLARE_VOID( get_last_irreversible_block_result, get_last_irreversible_block );
THUNK_DECLARE( get_account_nonce_result, get_account_nonce, const std::string& account );
THUNK_DECLARE( verify_account_nonce_result, verify_account_nonce, const std::string& account, const std::string& nonce );
THUNK_DECLARE( void, set_account_nonce, const std::string& account, const std::string& nonce );
THUNK_DECLARE( check_system_authority_result, check_system_authority, system_authorization_type type );

// Resource Subsystem

THUNK_DECLARE( get_account_rc_result, get_account_rc, const std::string& account );
THUNK_DECLARE( consume_account_rc_result, consume_account_rc, const std::string& account, uint64_t rc );
THUNK_DECLARE_VOID( get_resource_limits_result, get_resource_limits );
THUNK_DECLARE( consume_block_resources_result, consume_block_resources, uint64_t disk, uint64_t network, uint64_t compute );

// Database

THUNK_DECLARE( void, put_object, const object_space& space, const std::string& key, const std::string& obj );
THUNK_DECLARE( void, remove_object, const object_space& space, const std::string& key );
THUNK_DECLARE( get_object_result, get_object, const object_space& space, const std::string& key );
THUNK_DECLARE( get_next_object_result, get_next_object, const object_space& space, const std::string& key );
THUNK_DECLARE( get_prev_object_result, get_prev_object, const object_space& space, const std::string& key );

// Logging

THUNK_DECLARE( void, log, const std::string& msg );
THUNK_DECLARE( void, event, const std::string& name, const std::string& data, const std::vector< std::string >& impacted );

// Cryptography

THUNK_DECLARE( hash_result, hash, uint64_t code, const std::string& obj, uint64_t size = 0 );
THUNK_DECLARE( recover_public_key_result, recover_public_key, dsa type, const std::string& signature_data, const std::string& digest );
THUNK_DECLARE( verify_merkle_root_result, verify_merkle_root, const std::string& root, const std::vector< std::string >& hashes );
THUNK_DECLARE( verify_signature_result, verify_signature, dsa type, const std::string& public_key, const std::string& signature, const std::string& digest );
THUNK_DECLARE( verify_vrf_proof_result, verify_vrf_proof, dsa type, const std::string& public_key, const std::string& proof, const std::string& hash, const std::string& message );

// Contract Management

THUNK_DECLARE( call_result, call, const std::string& contract_id, uint32_t entry_point, const std::string& args );
THUNK_DECLARE( void, exit, result res );
THUNK_DECLARE_VOID( get_arguments_result, get_arguments );
THUNK_DECLARE_VOID( get_contract_id_result, get_contract_id );
THUNK_DECLARE_VOID( get_caller_result, get_caller );
THUNK_DECLARE( check_authority_result, check_authority, authorization_type type, const std::string& account );

} // koinos::chain
