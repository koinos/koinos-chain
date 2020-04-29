#include <koinos/exception.hpp>

namespace koinos::chain {

DECLARE_KOINOS_EXCEPTION( chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( chain_type_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( name_type_exception, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( public_key_type_exception, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( private_key_type_exception, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( authority_type_exception, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( action_type_exception, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( transaction_type_exception, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( abi_type_exception, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_id_type_exception, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( transaction_id_type_exception, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( packed_transaction_type_exception, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( asset_type_exception, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( chain_id_type_exception, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( fixed_key_type_exception, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( symbol_type_exception, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unactivated_key_type, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unactivated_signature_type, chain_type_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( fork_database_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( fork_db_block_not_found, fork_database_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_validate_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unlinkable_block_exception, block_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_tx_output_exception, block_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_concurrency_exception, block_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_lock_exception, block_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_resource_exhausted, block_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_too_old_exception, block_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_from_the_future, block_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( wrong_signing_key, block_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( wrong_producer, block_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_block_header_extension, block_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( ill_formed_protocol_feature_activation, block_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_block_extension, block_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( ill_formed_additional_block_signatures_extension, block_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( transaction_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( tx_decompression_error, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( tx_no_action, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( tx_no_auths, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( cfa_irrelevant_auth, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( expired_tx_exception, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( tx_exp_too_far_exception, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_ref_block_exception, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( tx_duplicate, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( deferred_tx_duplicate, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( cfa_inside_generated_tx, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( tx_not_found, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( too_many_tx_at_once, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( tx_too_big, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unknown_transaction_compression, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_transaction_extension, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( ill_formed_deferred_transaction_generation_context, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( disallowed_transaction_extensions_bad_block_exception, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( tx_resource_exhaustion, transaction_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( action_validate_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( account_name_exists_exception, action_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_action_args_exception, action_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( koinos_assert_message_exception, action_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( koinos_assert_code_exception, action_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( action_not_found_exception, action_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( action_data_and_struct_mismatch, action_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unaccessible_api, action_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( abort_called, action_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( inline_action_too_big, action_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unauthorized_ram_usage_increase, action_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( restricted_error_code_exception, action_validate_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( database_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( permission_query_exception, database_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( account_query_exception, database_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( contract_table_query_exception, database_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( contract_query_exception, database_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( bad_database_version_exception, database_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( guard_exception, database_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( database_guard_exception, guard_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( reversible_guard_exception, guard_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( wasm_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( page_memory_error, wasm_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( wasm_execution_error, wasm_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( wasm_serialization_error, wasm_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( overlapping_memory_error, wasm_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( binaryen_exception, wasm_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( resource_exhausted_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( ram_usage_exceeded, resource_exhausted_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( tx_net_usage_exceeded, resource_exhausted_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_net_usage_exceeded, resource_exhausted_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( tx_cpu_usage_exceeded, resource_exhausted_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_cpu_usage_exceeded, resource_exhausted_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( deadline_exception, resource_exhausted_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( greylist_net_usage_exceeded, resource_exhausted_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( greylist_cpu_usage_exceeded, resource_exhausted_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( leeway_deadline_exception, deadline_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( authorization_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( tx_duplicate_sig, authorization_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( tx_irrelevant_sig, authorization_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unsatisfied_authorization, authorization_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( missing_auth_exception, authorization_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( irrelevant_auth_exception, authorization_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( insufficient_delay_exception, authorization_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_permission, authorization_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unlinkable_min_permission_action, authorization_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_parent_permission, authorization_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( misc_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( rate_limiting_state_inconsistent, misc_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unknown_block_exception, misc_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unknown_transaction_exception, misc_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( fixed_reversible_db_exception, misc_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( extract_genesis_state_exception, misc_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( subjective_block_production_exception, misc_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( multiple_voter_info, misc_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unsupported_feature, misc_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( node_management_success, misc_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( json_parse_exception, misc_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( sig_variable_size_limit_exception, misc_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( plugin_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( missing_chain_api_plugin_exception, plugin_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( missing_wallet_api_plugin_exception, plugin_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( missing_history_api_plugin_exception, plugin_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( missing_net_api_plugin_exception, plugin_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( missing_chain_plugin_exception, plugin_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( plugin_config_exception, plugin_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( wallet_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( wallet_exist_exception, wallet_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( wallet_nonexistent_exception, wallet_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( wallet_locked_exception, wallet_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( wallet_missing_pub_key_exception, wallet_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( wallet_invalid_password_exception, wallet_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( wallet_not_available_exception, wallet_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( wallet_unlocked_exception, wallet_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( key_exist_exception, wallet_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( key_nonexistent_exception, wallet_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unsupported_key_type_exception, wallet_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_lock_timeout_exception, wallet_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( secure_enclave_exception, wallet_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( whitelist_blacklist_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( actor_whitelist_exception, whitelist_blacklist_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( actor_blacklist_exception, whitelist_blacklist_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( contract_whitelist_exception, whitelist_blacklist_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( contract_blacklist_exception, whitelist_blacklist_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( action_blacklist_exception, whitelist_blacklist_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( key_blacklist_exception, whitelist_blacklist_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( controller_emit_signal_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( checkpoint_exception, controller_emit_signal_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( abi_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( abi_not_found_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_ricardian_clause_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_ricardian_action_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_type_inside_abi, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( duplicate_abi_type_def_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( duplicate_abi_struct_def_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( duplicate_abi_action_def_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( duplicate_abi_table_def_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( duplicate_abi_err_msg_def_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( abi_serialization_deadline_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( abi_recursion_depth_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( abi_circular_def_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unpack_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( pack_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( duplicate_abi_variant_def_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unsupported_abi_version_exception, abi_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( contract_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_table_payer, contract_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( table_access_violation, contract_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_table_iterator, contract_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( table_not_in_cache, contract_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( table_operation_not_permitted, contract_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_contract_vm_type, contract_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_contract_vm_version, contract_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( set_exact_code, contract_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( wasm_file_not_found, contract_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( abi_file_not_found, contract_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( producer_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( producer_priv_key_not_found, producer_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( missing_pending_block_state, producer_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( producer_double_confirm, producer_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( producer_schedule_exception, producer_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( producer_not_in_schedule, producer_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( snapshot_directory_not_found_exception, producer_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( snapshot_exists_exception, producer_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( snapshot_finalization_exception, producer_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_protocol_features_to_activate, producer_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( no_block_signatures, producer_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( unsupported_multiple_block_signatures, producer_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( reversible_blocks_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_reversible_blocks_dir, reversible_blocks_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( reversible_blocks_backup_dir_exist, reversible_blocks_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( gap_in_reversible_blocks_db, reversible_blocks_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_log_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_log_unsupported_version, block_log_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_log_append_fail, block_log_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_log_not_found, block_log_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_log_backup_dir_exist, block_log_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( block_index_not_found, block_log_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( http_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_http_client_root_cert, http_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_http_response, http_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( resolved_to_multiple_ports, http_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( fail_to_resolve_host, http_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( http_request_fail, http_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( invalid_http_request, http_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( resource_limit_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( mongo_db_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( mongo_db_insert_fail, mongo_db_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( mongo_db_update_fail, mongo_db_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( contract_api_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( crypto_api_exception, contract_api_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( db_api_exception, contract_api_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( arithmetic_exception, contract_api_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( snapshot_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( snapshot_validation_exception, snapshot_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( protocol_feature_exception, chain_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( protocol_feature_validation_exception, protocol_feature_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( protocol_feature_bad_block_exception, protocol_feature_exception );

DECLARE_DERIVED_KOINOS_EXCEPTION( protocol_feature_iterator_exception, protocol_feature_exception );


} // koinos::chain
