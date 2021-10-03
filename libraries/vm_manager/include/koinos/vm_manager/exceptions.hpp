
#pragma once

#include <koinos/exception.hpp>

namespace koinos::vm_manager {

// Root of exception hierarchy for vm_manager library
KOINOS_DECLARE_EXCEPTION( vm_exception );

// Exceptions thrown by vm_manager
KOINOS_DECLARE_DERIVED_EXCEPTION( vm_manager_exception, vm_exception );
// Any particular backend should subclass all its exceptions from vmbackend_exception
KOINOS_DECLARE_DERIVED_EXCEPTION( vm_backend_exception, vm_exception );

KOINOS_DECLARE_DERIVED_EXCEPTION( tick_meter_exception, vm_manager_exception );

} // koinos::vm_manager
