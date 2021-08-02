#pragma once

#include <koinos/exception.hpp>
#include <koinos/vmmanager/exceptions.hpp>

namespace koinos::vmmanager::eos {

KOINOS_DECLARE_DERIVED_EXCEPTION( eos_vm_exception, vmbackend_exception );

// WASM exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( wasm_exception, eos_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( wasm_type_conversion_exception, eos_vm_exception );

}
