#pragma once

#include <koinos/exception.hpp>

namespace koinos::vmmanager {

// Root of exception hierarchy for vmmanager library
KOINOS_DECLARE_EXCEPTION( vm_exception );

// Exceptions thrown by vmmanager
KOINOS_DECLARE_DERIVED_EXCEPTION( vmmanager_exception, vm_exception );
// Any particular backend should subclass all its exceptions from vmbackend_exception
KOINOS_DECLARE_DERIVED_EXCEPTION( vmbackend_exception, vm_exception );

KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_backend_exception, vmmanager_exception );

}
