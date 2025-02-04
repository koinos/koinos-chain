#pragma once
#include <koinos/chain/error.pb.h>
#include <koinos/exception.hpp>

namespace koinos::chain {

KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( system_authorization_failure_exception,
                                            reversion_exception,
                                            system_authorization_failure );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( invalid_contract_exception, reversion_exception, invalid_contract );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( insufficient_privileges_exception,
                                            reversion_exception,
                                            insufficient_privileges );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( insufficient_rc_exception, reversion_exception, insufficient_rc );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( insufficient_return_buffer_exception,
                                            reversion_exception,
                                            insufficient_return_buffer );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unknown_thunk_exception, reversion_exception, unknown_thunk );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unknown_operation_exception, reversion_exception, unknown_operation );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( read_only_context_exception, reversion_exception, read_only_context );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( internal_error_exception, reversion_exception, internal_error );

KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( field_not_found_exception, failure_exception, field_not_found );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unknown_hash_code_exception, failure_exception, unknown_hash_code );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unknown_dsa_exception, failure_exception, unknown_dsa );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unknown_system_call_exception, failure_exception, unknown_system_call );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( operation_not_found_exception, failure_exception, operation_not_found );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( authorization_failure_exception, failure_exception, authorization_failure );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( invalid_nonce_exception, failure_exception, invalid_nonce );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( invalid_signature_exception, failure_exception, invalid_signature );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( malformed_block_exception, failure_exception, malformed_block );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( malformed_transaction_exception, failure_exception, malformed_transaction );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( block_resource_failure_exception,
                                            failure_exception,
                                            block_resource_failure );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( pending_transaction_limit_exceeded_exception,
                                            failure_exception,
                                            pending_transaction_limit_exceeded );

// Framework failures
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unknown_backend_exception, failure_exception, unknown_backend );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unexpected_state_exception, failure_exception, unexpected_state );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( missing_required_arguments_exception,
                                            failure_exception,
                                            missing_required_arguments );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unknown_previous_block_exception,
                                            failure_exception,
                                            unknown_previous_block );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unexpected_height_exception, failure_exception, unexpected_height );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( block_state_error_exception, failure_exception, block_state_error );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( state_merkle_mismatch_exception, failure_exception, state_merkle_mismatch );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( unexpected_receipt_exception, failure_exception, unexpected_receipt );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( rpc_failure_exception, failure_exception, rpc_failure );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( pending_state_error_exception, failure_exception, pending_state_error );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( timestamp_out_of_bounds_exception,
                                            failure_exception,
                                            timestamp_out_of_bounds );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( indexer_failure_exception, failure_exception, indexer_failure );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( network_bandwidth_limit_exceeded_exception,
                                            failure_exception,
                                            network_bandwidth_limit_exceeded );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( compute_bandwidth_limit_exceeded_exception,
                                            failure_exception,
                                            compute_bandwidth_limit_exceeded );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( disk_storage_limit_exceeded_exception,
                                            failure_exception,
                                            disk_storage_limit_exceeded );
KOINOS_DECLARE_DERIVED_EXCEPTION_WITH_CODE( pre_irreversibility_block_exception,
                                            failure_exception,
                                            pre_irreversibility_block );

} // namespace koinos::chain
