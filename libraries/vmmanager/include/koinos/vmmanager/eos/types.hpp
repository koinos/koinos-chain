#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wregister"

#ifndef __clang__
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <eosio/vm/backend.hpp>
#pragma GCC diagnostic pop
#include <eosio/vm/error_codes.hpp>
#include <eosio/vm/host_function.hpp>
#include <eosio/vm/exceptions.hpp>

#include <koinos/vmmanager/eos/apply_context.hpp>
#include <koinos/vmmanager/eos/type_conversion.hpp>

namespace koinos::vmmanager::eos {

   using wasm_allocator_type = eosio::vm::wasm_allocator;
   using backend_type        = eosio::vm::backend< eos_apply_context, eosio::vm::interpreter >;
   using registrar_type      = eosio::vm::registered_host_functions< eos_apply_context >;
   using wasm_code_ptr       = eosio::vm::wasm_code_ptr;

} // koinos::chain
