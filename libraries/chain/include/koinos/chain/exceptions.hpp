#pragma once
#include <koinos/exception.hpp>
#include <koinos/chain/error.pb.h>

namespace koinos::chain {

KOINOS_DECLARE_EXCEPTION_WITH_CODE( reversion_exception, reversion );
KOINOS_DECLARE_EXCEPTION_WITH_CODE( failure_exception, failure );

KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( system_authorization_failure_exception, reversion_exception, system_authorization_failure );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( invalid_contract_exception, reversion_exception, invalid_contract );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( insufficient_privileges_exception, reversion_exception, insufficient_privileges );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( insufficient_rc_exception, reversion_exception, insufficient_rc );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( insufficient_return_buffer_exception, reversion_exception, insufficient_return_buffer );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unknown_thunk_exception, reversion_exception, unknown_thunk );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unknown_operation_exception, reversion_exception, unknown_operation );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( read_only_context_exception, reversion_exception, read_only_context );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( internal_error_exception, reversion_exception, internal_error );

KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( field_not_found_exception, failure_exception, field_not_found );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unknown_hash_code_exception, failure_exception, unknown_hash_code );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unknown_dsa_exception, failure_exception, unknown_dsa );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unknown_system_call_exception, failure_exception, unknown_system_call );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( authorization_failure_exception, failure_exception, authorization_failure );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( invalid_nonce_exception, failure_exception, invalid_nonce );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( invalid_signature_exception, failure_exception, invalid_signature );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( malformed_block_exception, failure_exception, malformed_block );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( malformed_transaction_exception, failure_exception, malformed_transaction );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( block_resource_failure_exception, failure_exception, block_resource_failure );

KOINOS_DECLARE_EXCEPTION( chain_exception );

KOINOS_DECLARE_DERIVED_EXCEPTION( chain_reversion, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( chain_failure, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( chain_success, chain_exception );

// Database exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( database_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( state_node_not_found, database_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( out_of_bounds, database_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unexpected_state, database_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( retrieval_failure, database_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( insufficent_buffer_size, database_exception );

// Operation exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( operation_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_operation, operation_exception );

// System call exceptions
/*KOINOS_DECLARE_DERIVED_EXCEPTION( system_call_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( insufficient_return_buffer, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_system_call, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( invalid_contract, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( forbidden_override, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( exit_success, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( exit_failure, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_exit_code, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( insufficient_privileges, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_hash_code, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( empty_block_header, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( transaction_root_mismatch, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( operation_root_mismatch, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( passive_root_mismatch, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( invalid_block_signature, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( invalid_transaction_signature, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( invalid_signature, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( authorization_failed, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unimplemented_feature, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( thunk_not_found, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( thunk_not_enabled, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( read_only_context, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( transaction_reverted, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( invalid_dsa, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( invalid_nonce, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( block_id_mismatch, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( transaction_id_mismatch, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( chain_id_mismatch, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( hash_code_mismatch, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( merkle_hash_mismatch, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( invalid_argument, system_call_exception );*/

// Controller exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( controller_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unexpected_height, controller_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_previous_block, controller_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( duplicate_trx_state, controller_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( pending_state_error, controller_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( timestamp_out_of_bounds, controller_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( missing_required_arguments, controller_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( state_merkle_mismatch, controller_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( block_state_error, controller_exception );

// Stack exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( stack_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( stack_overflow, stack_exception );

// Resource exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( resource_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( insufficient_rc, resource_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unable_to_consume_resources, resource_exception );

// Block resource exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( block_resource_limit_exception, resource_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( network_bandwidth_limit_exceeded, block_resource_limit_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( compute_bandwidth_limit_exceeded, block_resource_limit_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( disk_storage_limit_exceeded, block_resource_limit_exception );

// VM exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_backend_exception, chain_exception );

// Parse exception
KOINOS_DECLARE_DERIVED_EXCEPTION( parse_failure, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unexpected_field_type, parse_failure );
KOINOS_DECLARE_DERIVED_EXCEPTION( unexpected_field_size, parse_failure );
KOINOS_DECLARE_DERIVED_EXCEPTION( field_not_found, parse_failure );

// Context exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( context_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unexpected_intent, context_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unexpected_access, context_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unexpected_receipt, context_exception );

// RPC exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( rpc_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( rpc_failure, rpc_exception );

// Indexer exception
KOINOS_DECLARE_DERIVED_EXCEPTION( indexer_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( indexer_failure, indexer_exception );

} // koinos::chain
