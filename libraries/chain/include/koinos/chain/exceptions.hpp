#pragma once
#include <koinos/exception.hpp>

namespace koinos::chain {

KOINOS_DECLARE_EXCEPTION( chain_exception );

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
KOINOS_DECLARE_DERIVED_EXCEPTION( system_call_exception, chain_exception );
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
KOINOS_DECLARE_DERIVED_EXCEPTION( unimplemented_feature, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( thunk_not_found, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( read_only_context, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( transaction_reverted, system_call_exception );

// Controller exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( controller_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unexpected_height, controller_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( block_id_mismatch, controller_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_previous_block, controller_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( duplicate_trx_state, controller_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( pending_state_error, controller_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( timestamp_out_of_bounds, controller_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( missing_required_arguments, controller_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( state_merkle_mismatch, controller_exception );

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

} // koinos::chain
