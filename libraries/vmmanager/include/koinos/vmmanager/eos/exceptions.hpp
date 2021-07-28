#pragma once

#include <koinos/exception.hpp>

namespace koinos::vmmanager::eos {

KOINOS_DECLARE_EXCEPTION( eos_vm_exception );

// WASM exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( wasm_exception, eos_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( wasm_type_conversion_exception, eos_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( insufficient_return_buffer, eos_vm_exception );

}
