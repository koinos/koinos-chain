#pragma once
#include <koinos/exception.hpp>

namespace koinos::chain {

DECLARE_KOINOS_EXCEPTION( chain_exception );

// Database exceptions
DECLARE_DERIVED_KOINOS_EXCEPTION( database_exception, chain_exception );

// Operation exceptions
DECLARE_DERIVED_KOINOS_EXCEPTION( operation_exception, chain_exception );
DECLARE_DERIVED_KOINOS_EXCEPTION( reserved_operation_exception, operation_exception );

// WASM exceptions
DECLARE_DERIVED_KOINOS_EXCEPTION( wasm_exception, chain_exception );
DECLARE_DERIVED_KOINOS_EXCEPTION( wasm_type_conversion_exception, wasm_exception );

} // koinos::chain
