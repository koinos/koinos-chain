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

namespace koinos::chain {

DECLARE_KOINOS_EXCEPTION( system_call_not_overridable );

// For any given system call, two slots are used. The first definition
// is considered overridable. The second system call slot is prefixed
// with an underscore to denote a private unoverridable implementation.

// When adding a system call slot, use the provided macro SYSTEM_CALL_SLOTS
// to declare both a public and private implementation.

SYSTEM_CALL_SLOTS(
   (register_syscall)
   (verify_block_header)
   (call_contract)

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
   (prints_l)
   (printi)
   (printui)
   (printi128)
   (printui128)
   (printsf)
   (printdf)
   (printqf)
   (printn)
   (printhex)

   (memset)
   (memcmp)
   (memmove)
   (memcpy)

   (current_receiver)
   (action_data_size)
   (read_action_data)

   (eosio_assert)
   (eosio_assert_message)
   (eosio_assert_code)
   (eosio_exit)
   (abort)

   (db_store_i64)
   (db_update_i64)
   (db_remove_i64)
   (db_get_i64)
   (db_next_i64)
   (db_previous_i64)
   (db_find_i64)
   (db_lowerbound_i64)
   (db_upperbound_i64)
   (db_end_i64)

   (db_idx64_store)
   (db_idx64_update)
   (db_idx64_remove)
   (db_idx64_next)
   (db_idx64_previous)
   (db_idx64_find_primary)
   (db_idx64_find_secondary)
   (db_idx64_lowerbound)
   (db_idx64_upperbound)
   (db_idx64_end)

   (db_idx128_store)
   (db_idx128_update)
   (db_idx128_remove)
   (db_idx128_next)
   (db_idx128_previous)
   (db_idx128_find_primary)
   (db_idx128_find_secondary)
   (db_idx128_lowerbound)
   (db_idx128_upperbound)
   (db_idx128_end)

   (db_idx256_store)
   (db_idx256_update)
   (db_idx256_remove)
   (db_idx256_next)
   (db_idx256_previous)
   (db_idx256_find_primary)
   (db_idx256_find_secondary)
   (db_idx256_lowerbound)
   (db_idx256_upperbound)
   (db_idx256_end)

   (db_idx_double_store)
   (db_idx_double_update)
   (db_idx_double_remove)
   (db_idx_double_next)
   (db_idx_double_previous)
   (db_idx_double_find_primary)
   (db_idx_double_find_secondary)
   (db_idx_double_lowerbound)
   (db_idx_double_upperbound)
   (db_idx_double_end)

   (db_idx_long_double_store)
   (db_idx_long_double_update)
   (db_idx_long_double_remove)
   (db_idx_long_double_next)
   (db_idx_long_double_previous)
   (db_idx_long_double_find_primary)
   (db_idx_long_double_find_secondary)
   (db_idx_long_double_lowerbound)
   (db_idx_long_double_upperbound)
   (db_idx_long_double_end)
);

struct system_call_bundle
{
   std::vector< uint8_t > wasm_bytes;
   name action;
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
      system_call_override_map pending_updates;

      bool overridable( system_call_slot s ) noexcept;
};

// When defining a new system call, we have essentially two different implementations.
// One of the implementations is considered upgradeable and can be overridden with
// VM code. The other implementation is prefixed with an underscore and cannot be
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

   // Assertions
   SYSTEM_CALL_DECLARE( void, abort );
   SYSTEM_CALL_DECLARE( void, eosio_assert, bool condition, null_terminated_ptr msg );
   SYSTEM_CALL_DECLARE( void, eosio_assert_message, bool, array_ptr< const char > msg, uint32_t len );
   SYSTEM_CALL_DECLARE( void, eosio_assert_code, bool condition, uint64_t error_code );
   SYSTEM_CALL_DECLARE( void, eosio_exit, int32_t code );

   SYSTEM_CALL_DECLARE( int, read_action_data, array_ptr<char> memory, uint32_t buffer_size );
   SYSTEM_CALL_DECLARE( int, action_data_size );
   SYSTEM_CALL_DECLARE( name, current_receiver );

   SYSTEM_CALL_DECLARE( char*, memcpy, array_ptr< char > dest, array_ptr< const char > src, uint32_t length );
   SYSTEM_CALL_DECLARE( char*, memmove, array_ptr< char > dest, array_ptr< const char > src, uint32_t length );
   SYSTEM_CALL_DECLARE( int, memcmp, array_ptr< const char > dest, array_ptr< const char > src, uint32_t length );
   SYSTEM_CALL_DECLARE( char*, memset, array_ptr<char> dest, int value, uint32_t length );

   SYSTEM_CALL_DECLARE( void, prints, null_terminated_ptr str );
   SYSTEM_CALL_DECLARE( void, prints_l, array_ptr< const char > str, uint32_t str_len );
   SYSTEM_CALL_DECLARE( void, printi, int64_t val );
   SYSTEM_CALL_DECLARE( void, printui, uint64_t val );
   SYSTEM_CALL_DECLARE( void, printi128, const __int128& val );
   SYSTEM_CALL_DECLARE( void, printui128, const unsigned __int128& val );
   SYSTEM_CALL_DECLARE( void, printsf, float val );
   SYSTEM_CALL_DECLARE( void, printdf, double val );
   SYSTEM_CALL_DECLARE( void, printqf, const float128_t& val );
   SYSTEM_CALL_DECLARE( void, printn, name val );
   SYSTEM_CALL_DECLARE( void, printhex, array_ptr< const char > data, uint32_t data_len );

   SYSTEM_CALL_DECLARE( int, db_store_i64, uint64_t scope, uint64_t table, uint64_t payer, uint64_t id, array_ptr< const char > buffer, uint32_t buffer_size );
   SYSTEM_CALL_DECLARE( void, db_update_i64, int itr, uint64_t payer, array_ptr< const char > buffer, uint32_t buffer_size );
   SYSTEM_CALL_DECLARE( void, db_remove_i64, int itr );
   SYSTEM_CALL_DECLARE( int, db_get_i64, int itr, array_ptr< char > buffer, uint32_t buffer_size );
   SYSTEM_CALL_DECLARE( int, db_next_i64, int itr, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_previous_i64, int itr, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_find_i64, uint64_t code, uint64_t scope, uint64_t table, uint64_t id );
   SYSTEM_CALL_DECLARE( int, db_lowerbound_i64, uint64_t code, uint64_t scope, uint64_t table, uint64_t id );
   SYSTEM_CALL_DECLARE( int, db_upperbound_i64, uint64_t code, uint64_t scope, uint64_t table, uint64_t id );
   SYSTEM_CALL_DECLARE( int, db_end_i64, uint64_t code, uint64_t scope, uint64_t table );

   SYSTEM_CALL_DECLARE( int, db_idx64_store, uint64_t scope, uint64_t table, uint64_t payer, uint64_t id, const uint64_t& secondary );
   SYSTEM_CALL_DECLARE( void, db_idx64_update, int iterator, uint64_t payer, const uint64_t& secondary );
   SYSTEM_CALL_DECLARE( void, db_idx64_remove, int iterator );
   SYSTEM_CALL_DECLARE( int, db_idx64_find_secondary, uint64_t code, uint64_t scope, uint64_t table, const uint64_t& secondary, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx64_find_primary, uint64_t code, uint64_t scope, uint64_t table, uint64_t& secondary, uint64_t primary );
   SYSTEM_CALL_DECLARE( int, db_idx64_lowerbound, uint64_t code, uint64_t scope, uint64_t table, uint64_t& secondary, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx64_upperbound, uint64_t code, uint64_t scope, uint64_t table, uint64_t& secondary, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx64_end, uint64_t code, uint64_t scope, uint64_t table );
   SYSTEM_CALL_DECLARE( int, db_idx64_next, int iterator, uint64_t& primary  );
   SYSTEM_CALL_DECLARE( int, db_idx64_previous, int iterator, uint64_t& primary );

   SYSTEM_CALL_DECLARE( int, db_idx128_store, uint64_t scope, uint64_t table, uint64_t payer, uint64_t id, const uint128_t& secondary );
   SYSTEM_CALL_DECLARE( void, db_idx128_update, int iterator, uint64_t payer, const uint128_t& secondary );
   SYSTEM_CALL_DECLARE( void, db_idx128_remove, int iterator );
   SYSTEM_CALL_DECLARE( int, db_idx128_find_secondary, uint64_t code, uint64_t scope, uint64_t table, const uint128_t& secondary, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx128_find_primary, uint64_t code, uint64_t scope, uint64_t table, uint128_t& secondary, uint64_t primary );
   SYSTEM_CALL_DECLARE( int, db_idx128_lowerbound, uint64_t code, uint64_t scope, uint64_t table, uint128_t& secondary, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx128_upperbound, uint64_t code, uint64_t scope, uint64_t table, uint128_t& secondary, uint64_t& primary );;
   SYSTEM_CALL_DECLARE( int, db_idx128_end, uint64_t code, uint64_t scope, uint64_t table );
   SYSTEM_CALL_DECLARE( int, db_idx128_next, int iterator, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx128_previous, int iterator, uint64_t& primary );

   SYSTEM_CALL_DECLARE( int, db_idx256_store, uint64_t scope, uint64_t table, uint64_t payer, uint64_t id, array_ptr< const uint128_t > data, uint32_t data_len );
   SYSTEM_CALL_DECLARE( void, db_idx256_update, int iterator, uint64_t payer, array_ptr< const uint128_t > data, uint32_t data_len );
   SYSTEM_CALL_DECLARE( void, db_idx256_remove, int iterator );
   SYSTEM_CALL_DECLARE( int, db_idx256_find_secondary, uint64_t code, uint64_t scope, uint64_t table, array_ptr< const uint128_t > data, uint32_t data_len, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx256_find_primary, uint64_t code, uint64_t scope, uint64_t table, array_ptr< uint128_t > data, uint32_t data_len, uint64_t primary );
   SYSTEM_CALL_DECLARE( int, db_idx256_lowerbound, uint64_t code, uint64_t scope, uint64_t table, array_ptr< uint128_t > data, uint32_t data_len, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx256_upperbound, uint64_t code, uint64_t scope, uint64_t table, array_ptr< uint128_t > data, uint32_t data_len, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx256_end, uint64_t code, uint64_t scope, uint64_t table );
   SYSTEM_CALL_DECLARE( int, db_idx256_next, int iterator, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx256_previous, int iterator, uint64_t& primary );

   SYSTEM_CALL_DECLARE( int, db_idx_double_store, uint64_t scope, uint64_t table, uint64_t payer, uint64_t id, const float64_t& secondary );
   SYSTEM_CALL_DECLARE( void, db_idx_double_update, int iterator, uint64_t payer, const float64_t& secondary );
   SYSTEM_CALL_DECLARE( void, db_idx_double_remove, int iterator );
   SYSTEM_CALL_DECLARE( int, db_idx_double_find_secondary, uint64_t code, uint64_t scope, uint64_t table, const float64_t& secondary, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx_double_find_primary, uint64_t code, uint64_t scope, uint64_t table, float64_t& secondary, uint64_t primary );
   SYSTEM_CALL_DECLARE( int, db_idx_double_lowerbound, uint64_t code, uint64_t scope, uint64_t table, float64_t& secondary, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx_double_upperbound, uint64_t code, uint64_t scope, uint64_t table, float64_t& secondary, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx_double_end, uint64_t code, uint64_t scope, uint64_t table );
   SYSTEM_CALL_DECLARE( int, db_idx_double_next, int iterator, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx_double_previous, int iterator, uint64_t& primary );

   SYSTEM_CALL_DECLARE( int, db_idx_long_double_store, uint64_t scope, uint64_t table, uint64_t payer, uint64_t id, const float128_t& secondary );
   SYSTEM_CALL_DECLARE( void, db_idx_long_double_update, int iterator, uint64_t payer, const float128_t& secondary );
   SYSTEM_CALL_DECLARE( void, db_idx_long_double_remove, int iterator );
   SYSTEM_CALL_DECLARE( int, db_idx_long_double_find_secondary, uint64_t code, uint64_t scope, uint64_t table, const float128_t& secondary, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx_long_double_find_primary, uint64_t code, uint64_t scope, uint64_t table, float128_t& secondary, uint64_t primary );
   SYSTEM_CALL_DECLARE( int, db_idx_long_double_lowerbound, uint64_t code, uint64_t scope, uint64_t table, float128_t& secondary, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx_long_double_upperbound, uint64_t code, uint64_t scope, uint64_t table, float128_t& secondary, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx_long_double_end, uint64_t code, uint64_t scope, uint64_t table );
   SYSTEM_CALL_DECLARE( int, db_idx_long_double_next, int iterator, uint64_t& primary );
   SYSTEM_CALL_DECLARE( int, db_idx_long_double_previous, int iterator, uint64_t& primary );
};

} // koinos::chain
