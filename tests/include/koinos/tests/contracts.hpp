#pragma once

#include <string>

const std::string& get_authorize_wasm();
const std::string& get_benchmark_wasm();
const std::string& get_contract_return_wasm();
const std::string& get_db_write_wasm();
const std::string& get_empty_contract_wasm();
const std::string& get_forever_wasm();
const std::string& get_hello_wasm();
const std::string& get_koin_wasm();
const std::string& get_syscall_override_wasm();

const std::string& get_call_contract_wasm();
const std::string& get_call_system_call_wasm();
const std::string& get_call_system_call2_wasm();
const std::string& get_stack_assertion_wasm();
const std::string& get_system_from_system_wasm();
const std::string& get_system_from_user_wasm();
const std::string& get_user_from_system_wasm();
const std::string& get_user_from_user_wasm();
