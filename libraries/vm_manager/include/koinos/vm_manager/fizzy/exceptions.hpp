#pragma once

#include <koinos/exception.hpp>
#include <koinos/vm_manager/exceptions.hpp>

namespace koinos::vm_manager::fizzy {

KOINOS_DECLARE_DERIVED_EXCEPTION( fizzy_vm_exception, vm_backend_exception );

// Module loading exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( module_parse_exception, fizzy_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( module_instantiate_exception, fizzy_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( create_context_exception, fizzy_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( module_start_exception, fizzy_vm_exception );

// Runtime exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( wasm_trap_exception, fizzy_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( wasm_memory_exception, fizzy_vm_exception );

// These exceptions should never happen, if they do it's a programming bug
KOINOS_DECLARE_DERIVED_EXCEPTION( fizzy_returned_null_exception, fizzy_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( null_argument_exception, fizzy_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( runner_state_exception, fizzy_vm_exception );

} // koinos::vm_manager::fizzy
