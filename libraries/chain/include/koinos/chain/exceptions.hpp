#pragma once
#include <koinos/exception.hpp>

namespace koinos::chain {

KOINOS_DECLARE_EXCEPTION( chain_exception );

// Database exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( database_exception, chain_exception );

// Operation exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( operation_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( reserved_operation_exception, operation_exception );

// WASM exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( wasm_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( wasm_type_conversion_exception, wasm_exception );

// Thunk exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( thunk_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unimplemented, thunk_exception );

// System call exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( system_call_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_system_call, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( invalid_contract, system_call_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( forbidden_override, system_call_exception );

} // koinos::chain
