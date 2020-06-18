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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-variable"

#ifndef __clang__
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <eosio/vm/backend.hpp>
#pragma GCC diagnostic pop
#include <eosio/vm/error_codes.hpp>
#include <eosio/vm/host_function.hpp>
#include <eosio/vm/exceptions.hpp>
#include <koinos/chain/wasm/type_conversion.hpp>

namespace koinos::chain {

DECLARE_KOINOS_EXCEPTION( system_call_not_overridable );

using wasm_allocator_type = eosio::vm::wasm_allocator;
using backend_type        = eosio::vm::backend< apply_context >;
using registrar_type      = eosio::vm::registered_host_functions< apply_context >;
using wasm_code_ptr       = eosio::vm::wasm_code_ptr;

using variable_blob       = koinos::protocol::variable_blob;

// When defining a new system call, we have essentially two different implementations.
// One of the implementations is considered upgradeable and can be overridden with
// VM code. The other implementation is prefixed with `internal_` and cannot be
// overridden and is the default implement if no override is provided.

// Use the macro SYSTEM_CALL_DECLARE to simultaneously declare both public and private versions.

struct system_api final
{
   system_api( apply_context& ctx );
   apply_context& context;

   // Compiler Builtins
   SYSTEM_CALL_DECLARE( void, __ashlti3, __int128& ret, uint64_t low, uint64_t high, uint32_t shift );
   SYSTEM_CALL_DECLARE( void, __ashrti3, __int128& ret, uint64_t low, uint64_t high, uint32_t shift );
   SYSTEM_CALL_DECLARE( void, __lshlti3, __int128& ret, uint64_t low, uint64_t high, uint32_t shift );
   SYSTEM_CALL_DECLARE( void, __lshrti3, __int128& ret, uint64_t low, uint64_t high, uint32_t shift );
   SYSTEM_CALL_DECLARE( void, __divti3, __int128& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( void, __udivti3, unsigned __int128& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( void, __multi3, __int128& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( void, __modti3 ,__int128& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( void, __umodti3, unsigned __int128& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( void, __addtf3, float128_t& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( void, __subtf3, float128_t& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( void, __multf3, float128_t& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( void, __divtf3, float128_t& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( void, __negtf2, float128_t& ret, uint64_t la, uint64_t ha );
   SYSTEM_CALL_DECLARE( void, __extendsftf2, float128_t& ret, float f );
   SYSTEM_CALL_DECLARE( void, __extenddftf2, float128_t& ret, double d );
   SYSTEM_CALL_DECLARE( double, __trunctfdf2, uint64_t l, uint64_t h );
   SYSTEM_CALL_DECLARE( float, __trunctfsf2, uint64_t l, uint64_t h );
   SYSTEM_CALL_DECLARE( int32_t, __fixtfsi, uint64_t l, uint64_t h );
   SYSTEM_CALL_DECLARE( int64_t, __fixtfdi, uint64_t l, uint64_t h );
   SYSTEM_CALL_DECLARE( void, __fixtfti, __int128& ret, uint64_t l, uint64_t h );
   SYSTEM_CALL_DECLARE( uint32_t, __fixunstfsi, uint64_t l, uint64_t h );
   SYSTEM_CALL_DECLARE( uint64_t, __fixunstfdi, uint64_t l, uint64_t h );
   SYSTEM_CALL_DECLARE( void, __fixunstfti, unsigned __int128& ret, uint64_t l, uint64_t h );
   SYSTEM_CALL_DECLARE( void, __fixsfti, __int128& ret, float a );
   SYSTEM_CALL_DECLARE( void, __fixdfti, __int128& ret, double a );
   SYSTEM_CALL_DECLARE( void, __fixunssfti, unsigned __int128& ret, float a );
   SYSTEM_CALL_DECLARE( void, __fixunsdfti, unsigned __int128& ret, double a );
   SYSTEM_CALL_DECLARE( double, __floatsidf, int32_t i );
   SYSTEM_CALL_DECLARE( void, __floatsitf, float128_t& ret, int32_t i );
   SYSTEM_CALL_DECLARE( void, __floatditf, float128_t& ret, uint64_t a );
   SYSTEM_CALL_DECLARE( void, __floatunsitf, float128_t& ret, uint32_t i );
   SYSTEM_CALL_DECLARE( void, __floatunditf, float128_t& ret, uint64_t a );
   SYSTEM_CALL_DECLARE( double, __floattidf, uint64_t l, uint64_t h );
   SYSTEM_CALL_DECLARE( double, __floatuntidf, uint64_t l, uint64_t h );
   SYSTEM_CALL_DECLARE( int, ___cmptf2, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb, int return_value_if_nan );
   SYSTEM_CALL_DECLARE( int, __eqtf2, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( int, __netf2, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( int, __getf2, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( int, __gttf2, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( int, __letf2, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( int, __lttf2, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( int, __cmptf2, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );
   SYSTEM_CALL_DECLARE( int, __unordtf2, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb );

   // Soft Float
   SYSTEM_CALL_DECLARE( float, _eosio_f32_add, float a, float b );
   SYSTEM_CALL_DECLARE( float, _eosio_f32_sub, float a, float b );
   SYSTEM_CALL_DECLARE( float, _eosio_f32_div, float a, float b );
   SYSTEM_CALL_DECLARE( float, _eosio_f32_mul, float a, float b );
   SYSTEM_CALL_DECLARE( float, _eosio_f32_min, float af, float bf );
   SYSTEM_CALL_DECLARE( float, _eosio_f32_max, float af, float bf );
   SYSTEM_CALL_DECLARE( float, _eosio_f32_copysign, float af, float bf );
   SYSTEM_CALL_DECLARE( float, _eosio_f32_abs, float af );
   SYSTEM_CALL_DECLARE( float, _eosio_f32_neg, float af );
   SYSTEM_CALL_DECLARE( float, _eosio_f32_sqrt, float a );
   SYSTEM_CALL_DECLARE( float, _eosio_f32_ceil, float af );
   SYSTEM_CALL_DECLARE( float, _eosio_f32_floor, float af );
   SYSTEM_CALL_DECLARE( float, _eosio_f32_trunc, float af );
   SYSTEM_CALL_DECLARE( float, _eosio_f32_nearest, float af );
   SYSTEM_CALL_DECLARE( bool, _eosio_f32_eq, float a, float b );
   SYSTEM_CALL_DECLARE( bool, _eosio_f32_ne, float a, float b );
   SYSTEM_CALL_DECLARE( bool, _eosio_f32_lt, float a, float b );
   SYSTEM_CALL_DECLARE( bool, _eosio_f32_le, float a, float b );
   SYSTEM_CALL_DECLARE( bool, _eosio_f32_gt, float af, float bf );
   SYSTEM_CALL_DECLARE( bool, _eosio_f32_ge, float af, float bf );
   SYSTEM_CALL_DECLARE( double, _eosio_f64_add, double a, double b );
   SYSTEM_CALL_DECLARE( double, _eosio_f64_sub, double a, double b );
   SYSTEM_CALL_DECLARE( double, _eosio_f64_div, double a, double b );
   SYSTEM_CALL_DECLARE( double, _eosio_f64_mul, double a, double b );
   SYSTEM_CALL_DECLARE( double, _eosio_f64_min, double af, double bf );
   SYSTEM_CALL_DECLARE( double, _eosio_f64_max, double af, double bf );
   SYSTEM_CALL_DECLARE( double, _eosio_f64_copysign, double af, double bf );
   SYSTEM_CALL_DECLARE( double, _eosio_f64_abs, double af );
   SYSTEM_CALL_DECLARE( double, _eosio_f64_neg, double af );
   SYSTEM_CALL_DECLARE( double, _eosio_f64_sqrt, double a );
   SYSTEM_CALL_DECLARE( double, _eosio_f64_ceil, double af );
   SYSTEM_CALL_DECLARE( double, _eosio_f64_floor, double af );
   SYSTEM_CALL_DECLARE( double, _eosio_f64_trunc, double af );
   SYSTEM_CALL_DECLARE( double, _eosio_f64_nearest, double af );
   SYSTEM_CALL_DECLARE( bool, _eosio_f64_eq, double a, double b );
   SYSTEM_CALL_DECLARE( bool, _eosio_f64_ne, double a, double b );
   SYSTEM_CALL_DECLARE( bool, _eosio_f64_lt, double a, double b );
   SYSTEM_CALL_DECLARE( bool, _eosio_f64_le, double a, double b );
   SYSTEM_CALL_DECLARE( bool, _eosio_f64_gt, double af, double bf );
   SYSTEM_CALL_DECLARE( bool, _eosio_f64_ge, double af, double bf );
   SYSTEM_CALL_DECLARE( double, _eosio_f32_promote, float a );
   SYSTEM_CALL_DECLARE( float, _eosio_f64_demote, double a );
   SYSTEM_CALL_DECLARE( int32_t, _eosio_f32_trunc_i32s, float af );
   SYSTEM_CALL_DECLARE( int32_t, _eosio_f64_trunc_i32s, double af );
   SYSTEM_CALL_DECLARE( uint32_t, _eosio_f32_trunc_i32u, float af );
   SYSTEM_CALL_DECLARE( uint32_t, _eosio_f64_trunc_i32u, double af );
   SYSTEM_CALL_DECLARE( int64_t, _eosio_f32_trunc_i64s, float af );
   SYSTEM_CALL_DECLARE( int64_t, _eosio_f64_trunc_i64s, double af );
   SYSTEM_CALL_DECLARE( uint64_t, _eosio_f32_trunc_i64u, float af );
   SYSTEM_CALL_DECLARE( uint64_t, _eosio_f64_trunc_i64u, double af );
   SYSTEM_CALL_DECLARE( float, _eosio_i32_to_f32, int32_t a );
   SYSTEM_CALL_DECLARE( float, _eosio_i64_to_f32, int64_t a );
   SYSTEM_CALL_DECLARE( float, _eosio_ui32_to_f32, uint32_t a );
   SYSTEM_CALL_DECLARE( float, _eosio_ui64_to_f32, uint64_t a );
   SYSTEM_CALL_DECLARE( double, _eosio_i32_to_f64, int32_t a );
   SYSTEM_CALL_DECLARE( double, _eosio_i64_to_f64, int64_t a );
   SYSTEM_CALL_DECLARE( double, _eosio_ui32_to_f64, uint32_t a );
   SYSTEM_CALL_DECLARE( double, _eosio_ui64_to_f64, uint64_t a );

   SYSTEM_CALL_DECLARE( void, prints, null_terminated_ptr str );

   SYSTEM_CALL_DECLARE( bool, verify_block_header, const crypto::recoverable_signature& sig, const crypto::multihash_type& digest );

   SYSTEM_CALL_DECLARE( void, apply_block, const protocol::active_block_data& b );
   SYSTEM_CALL_DECLARE( void, apply_transaction, const protocol::transaction_type& t );
   SYSTEM_CALL_DECLARE( void, apply_upload_contract_operation, const protocol::create_system_contract_operation& o );
   SYSTEM_CALL_DECLARE( void, apply_execute_contract_operation, const protocol::contract_call_operation& op );

   SYSTEM_CALL_DECLARE( bool, db_put_object, const statedb::object_space& space, const statedb::object_key& key, const variable_blob& obj );
   SYSTEM_CALL_DECLARE( variable_blob, db_get_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );
   SYSTEM_CALL_DECLARE( variable_blob, db_get_next_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );
   SYSTEM_CALL_DECLARE( variable_blob, db_get_prev_object, const statedb::object_space& space, const statedb::object_key& key, int32_t object_size_hint = -1 );

   SYSTEM_CALL_DECLARE( uint32_t, contract_args_size );
   SYSTEM_CALL_DECLARE( uint32_t, read_contract_args, array_ptr<char> memory, uint32_t buffer_size );
};

// For any given system call, two slots are used. The first definition
// is considered overridable. The second system call slot is prefixed
// with `internal_` to denote a private unoverridable implementation.

// When adding a system call slot, use the provided macro SYSTEM_CALL_SLOTS
// to declare both a public and private implementation.

SYSTEM_CALL_SLOTS(
   (__ashlti3)
   (__ashrti3)
   (__lshlti3)
   (__lshrti3)
   (__divti3)
   (__udivti3)
   (__multi3)
   (__modti3)
   (__umodti3)
   (__addtf3)
   (__subtf3)
   (__multf3)
   (__divtf3)
   (__negtf2)
   (__extendsftf2)
   (__extenddftf2)
   (__trunctfdf2)
   (__trunctfsf2)
   (__fixtfsi)
   (__fixtfdi)
   (__fixtfti)
   (__fixunstfsi)
   (__fixunstfdi)
   (__fixunstfti)
   (__fixsfti)
   (__fixdfti)
   (__fixunssfti)
   (__fixunsdfti)
   (__floatsidf)
   (__floatsitf)
   (__floatditf)
   (__floatunsitf)
   (__floatunditf)
   (__floattidf)
   (__floatuntidf)
   (___cmptf2)
   (__eqtf2)
   (__netf2)
   (__getf2)
   (__gttf2)
   (__letf2)
   (__lttf2)
   (__cmptf2)
   (__unordtf2)

   (_eosio_f32_add)
   (_eosio_f32_sub)
   (_eosio_f32_div)
   (_eosio_f32_mul)
   (_eosio_f32_min)
   (_eosio_f32_max)
   (_eosio_f32_copysign)
   (_eosio_f32_abs)
   (_eosio_f32_neg)
   (_eosio_f32_sqrt)
   (_eosio_f32_ceil)
   (_eosio_f32_floor)
   (_eosio_f32_trunc)
   (_eosio_f32_nearest)
   (_eosio_f32_eq)
   (_eosio_f32_ne)
   (_eosio_f32_lt)
   (_eosio_f32_le)
   (_eosio_f32_gt)
   (_eosio_f32_ge)
   (_eosio_f64_add)
   (_eosio_f64_sub)
   (_eosio_f64_div)
   (_eosio_f64_mul)
   (_eosio_f64_min)
   (_eosio_f64_max)
   (_eosio_f64_copysign)
   (_eosio_f64_abs)
   (_eosio_f64_neg)
   (_eosio_f64_sqrt)
   (_eosio_f64_ceil)
   (_eosio_f64_floor)
   (_eosio_f64_trunc)
   (_eosio_f64_nearest)
   (_eosio_f64_eq)
   (_eosio_f64_ne)
   (_eosio_f64_lt)
   (_eosio_f64_le)
   (_eosio_f64_gt)
   (_eosio_f64_ge)
   (_eosio_f32_promote)
   (_eosio_f64_demote)
   (_eosio_f32_trunc_i32s)
   (_eosio_f64_trunc_i32s)
   (_eosio_f32_trunc_i32u)
   (_eosio_f64_trunc_i32u)
   (_eosio_f32_trunc_i64s)
   (_eosio_f64_trunc_i64s)
   (_eosio_f32_trunc_i64u)
   (_eosio_f64_trunc_i64u)
   (_eosio_i32_to_f32)
   (_eosio_i64_to_f32)
   (_eosio_ui32_to_f32)
   (_eosio_ui64_to_f32)
   (_eosio_i32_to_f64)
   (_eosio_i64_to_f64)
   (_eosio_ui32_to_f64)
   (_eosio_ui64_to_f64)

   (prints)

// We might want to move these calls later, but append them for now
   (verify_block_header)

   (apply_block)
   (apply_transaction)
   (apply_upload_contract_operation)
   (apply_execute_contract_operation)

   (db_put_object)
   (db_get_object)
   (db_get_next_object)
   (db_get_prev_object)

   (contract_args_size)
   (read_contract_args)
);

struct system_call_bundle
{
   std::vector< uint8_t > wasm_bytes;
};

class system_call_table final
{
   public:
      void update();
      void set_system_call( system_call_slot s, system_call_bundle v );
      std::optional< system_call_bundle > get_system_call( system_call_slot s );

   private:
      using system_call_override_map = std::map< system_call_slot, system_call_bundle >;
      system_call_override_map system_call_map;

      KOINOS_TODO( "Replace pending_updates with a state that tracks undo and such from chain operations" )
      system_call_override_map pending_updates;

      bool overridable( system_call_slot s ) noexcept;
};

} // koinos::chain
