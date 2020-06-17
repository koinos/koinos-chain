#pragma once
#include <cstdint>
#include <optional>
#include <map>
#include <vector>
#include <cstring>

#include <compiler_builtins.hpp>
#include <koinos/exception.hpp>
#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/chain/system_call_utils.hpp>
#include <koinos/chain/wasm/common.hpp>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/pack/classes.hpp>
#include <koinos/statedb/statedb.hpp>


namespace koinos::chain {

struct system_api final
{
   system_api( apply_context& ctx );
   apply_context& context;

   void invoke_thunk( uint32_t tid, array_ptr< char > ret_ptr, uint32_t ret_len, array_ptr< const char > arg_ptr, uint32_t arg_len );
   void invoke_xcall( uint32_t xid, array_ptr< char > ret_ptr, uint32_t ret_len, array_ptr< const char > arg_ptr, uint32_t arg_len );
};

inline void register_host_functions()
{
   registrar_type::add< system_api, &system_api::invoke_thunk, wasm_allocator_type >( "env", "invoke_thunk" );
   registrar_type::add< system_api, &system_api::invoke_xcall, wasm_allocator_type >( "env", "invoke_xcall" );
}

} // koinos::chain
