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

#include <koinos/pack/rt/pack_fwd.hpp>
#include <koinos/chain/wasm/type_conversion.hpp>

#include <memory>
#include <vector>
#include <deque>
#include <cstdint>

namespace koinos::chain {

   using std::map;
   using std::vector;
   using std::string;
   using std::pair;
   using std::make_pair;

   using wasm_allocator_type = eosio::vm::wasm_allocator;
   using backend_type        = eosio::vm::backend< apply_context, eosio::vm::jit >;
   using registrar_type      = eosio::vm::registered_host_functions< apply_context >;
   using wasm_code_ptr       = eosio::vm::wasm_code_ptr;

} // koinos::chain
