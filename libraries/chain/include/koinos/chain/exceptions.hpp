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

} // koinos::chain
