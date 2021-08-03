#pragma once

#include <koinos/exception.hpp>
#include <koinos/vmmanager/exceptions.hpp>

namespace koinos::vmmanager::fizzy {

KOINOS_DECLARE_DERIVED_EXCEPTION( fizzy_vm_exception, vmbackend_exception );

// Module loading exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( module_parse_exception, fizzy_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( module_instantiate_exception, fizzy_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( create_context_exception, fizzy_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( module_start_exception, fizzy_vm_exception );

// Runtime exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( wasm_trap_exception, fizzy_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( wasm_memory_exception, fizzy_vm_exception );

}
