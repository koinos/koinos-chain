#pragma once
#include <cstdint>
#include <optional>
#include <map>
#include <vector>

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

// When adding a system call slot, use the provided macro SYSTEM_CALL_SLOT
// to declare both a public and private implementation.

enum class system_call_slot : uint32_t
{
   SYSTEM_CALL_SLOT(register_syscall),
   SYSTEM_CALL_SLOT(verify_block_header),
   SYSTEM_CALL_SLOT(call_contract),

   SYSTEM_CALL_SLOT(prints),
   SYSTEM_CALL_SLOT(prints_l),
   SYSTEM_CALL_SLOT(printi),
   SYSTEM_CALL_SLOT(printui),
   SYSTEM_CALL_SLOT(printi128),
   SYSTEM_CALL_SLOT(printui128),
   SYSTEM_CALL_SLOT(printsf),
   SYSTEM_CALL_SLOT(printdf),
   SYSTEM_CALL_SLOT(printqf),
   SYSTEM_CALL_SLOT(printn),
   SYSTEM_CALL_SLOT(printhex),

   SYSTEM_CALL_SLOT(memset),
   SYSTEM_CALL_SLOT(memcmp),
   SYSTEM_CALL_SLOT(memmove),
   SYSTEM_CALL_SLOT(memcpy),

   SYSTEM_CALL_SLOT(current_receiver),
   SYSTEM_CALL_SLOT(action_data_size),
   SYSTEM_CALL_SLOT(read_action_data),

   SYSTEM_CALL_SLOT(eosio_assert),
   SYSTEM_CALL_SLOT(eosio_assert_message),
   SYSTEM_CALL_SLOT(eosio_assert_code),
   SYSTEM_CALL_SLOT(eosio_exit),
   SYSTEM_CALL_SLOT(abort),

   SYSTEM_CALL_SLOT(db_store_i64),
   SYSTEM_CALL_SLOT(db_update_i64),
   SYSTEM_CALL_SLOT(db_remove_i64),
   SYSTEM_CALL_SLOT(db_get_i64),
   SYSTEM_CALL_SLOT(db_next_i64),
   SYSTEM_CALL_SLOT(db_previous_i64),
   SYSTEM_CALL_SLOT(db_find_i64),
   SYSTEM_CALL_SLOT(db_lowerbound_i64),
   SYSTEM_CALL_SLOT(db_upperbound_i64),
   SYSTEM_CALL_SLOT(db_end_i64),

   SYSTEM_CALL_SLOT(db_idx64_store),
   SYSTEM_CALL_SLOT(db_idx64_update),
   SYSTEM_CALL_SLOT(db_idx64_remove),
   SYSTEM_CALL_SLOT(db_idx64_next),
   SYSTEM_CALL_SLOT(db_idx64_previous),
   SYSTEM_CALL_SLOT(db_idx64_find_primary),
   SYSTEM_CALL_SLOT(db_idx64_find_secondary),
   SYSTEM_CALL_SLOT(db_idx64_lowerbound),
   SYSTEM_CALL_SLOT(db_idx64_upperbound),
   SYSTEM_CALL_SLOT(db_idx64_end),

   SYSTEM_CALL_SLOT(db_idx128_store),
   SYSTEM_CALL_SLOT(db_idx128_update),
   SYSTEM_CALL_SLOT(db_idx128_remove),
   SYSTEM_CALL_SLOT(db_idx128_next),
   SYSTEM_CALL_SLOT(db_idx128_previous),
   SYSTEM_CALL_SLOT(db_idx128_find_primary),
   SYSTEM_CALL_SLOT(db_idx128_find_secondary),
   SYSTEM_CALL_SLOT(db_idx128_lowerbound),
   SYSTEM_CALL_SLOT(db_idx128_upperbound),
   SYSTEM_CALL_SLOT(db_idx128_end),

   SYSTEM_CALL_SLOT(db_idx256_store),
   SYSTEM_CALL_SLOT(db_idx256_update),
   SYSTEM_CALL_SLOT(db_idx256_remove),
   SYSTEM_CALL_SLOT(db_idx256_next),
   SYSTEM_CALL_SLOT(db_idx256_previous),
   SYSTEM_CALL_SLOT(db_idx256_find_primary),
   SYSTEM_CALL_SLOT(db_idx256_find_secondary),
   SYSTEM_CALL_SLOT(db_idx256_lowerbound),
   SYSTEM_CALL_SLOT(db_idx256_upperbound),
   SYSTEM_CALL_SLOT(db_idx256_end),

   SYSTEM_CALL_SLOT(db_idx_double_store),
   SYSTEM_CALL_SLOT(db_idx_double_update),
   SYSTEM_CALL_SLOT(db_idx_double_remove),
   SYSTEM_CALL_SLOT(db_idx_double_next),
   SYSTEM_CALL_SLOT(db_idx_double_previous),
   SYSTEM_CALL_SLOT(db_idx_double_find_primary),
   SYSTEM_CALL_SLOT(db_idx_double_find_secondary),
   SYSTEM_CALL_SLOT(db_idx_double_lowerbound),
   SYSTEM_CALL_SLOT(db_idx_double_upperbound),
   SYSTEM_CALL_SLOT(db_idx_double_end),

   SYSTEM_CALL_SLOT(db_idx_long_double_store),
   SYSTEM_CALL_SLOT(db_idx_long_double_update),
   SYSTEM_CALL_SLOT(db_idx_long_double_remove),
   SYSTEM_CALL_SLOT(db_idx_long_double_next),
   SYSTEM_CALL_SLOT(db_idx_long_double_previous),
   SYSTEM_CALL_SLOT(db_idx_long_double_find_primary),
   SYSTEM_CALL_SLOT(db_idx_long_double_find_secondary),
   SYSTEM_CALL_SLOT(db_idx_long_double_lowerbound),
   SYSTEM_CALL_SLOT(db_idx_long_double_upperbound),
   SYSTEM_CALL_SLOT(db_idx_long_double_end)
};

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

class softfloat_api  {
   public:
      // TODO add traps on truncations for special cases (NaN or outside the range which rounds to an integer)
      softfloat_api( apply_context& ctx ){}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
      // float binops
      float _eosio_f32_add( float a, float b ) {
         float32_t ret = ::f32_add( to_softfloat32(a), to_softfloat32(b) );
         return *reinterpret_cast<float*>(&ret);
      }
      float _eosio_f32_sub( float a, float b ) {
         float32_t ret = ::f32_sub( to_softfloat32(a), to_softfloat32(b) );
         return *reinterpret_cast<float*>(&ret);
      }
      float _eosio_f32_div( float a, float b ) {
         float32_t ret = ::f32_div( to_softfloat32(a), to_softfloat32(b) );
         return *reinterpret_cast<float*>(&ret);
      }
      float _eosio_f32_mul( float a, float b ) {
         float32_t ret = ::f32_mul( to_softfloat32(a), to_softfloat32(b) );
         return *reinterpret_cast<float*>(&ret);
      }
#pragma GCC diagnostic pop
      float _eosio_f32_min( float af, float bf ) {
         float32_t a = to_softfloat32(af);
         float32_t b = to_softfloat32(bf);
         if (is_nan(a)) {
            return af;
         }
         if (is_nan(b)) {
            return bf;
         }
         if ( f32_sign_bit(a) != f32_sign_bit(b) ) {
            return f32_sign_bit(a) ? af : bf;
         }
         return ::f32_lt(a,b) ? af : bf;
      }
      float _eosio_f32_max( float af, float bf ) {
         float32_t a = to_softfloat32(af);
         float32_t b = to_softfloat32(bf);
         if (is_nan(a)) {
            return af;
         }
         if (is_nan(b)) {
            return bf;
         }
         if ( f32_sign_bit(a) != f32_sign_bit(b) ) {
            return f32_sign_bit(a) ? bf : af;
         }
         return ::f32_lt( a, b ) ? bf : af;
      }
      float _eosio_f32_copysign( float af, float bf ) {
         float32_t a = to_softfloat32(af);
         float32_t b = to_softfloat32(bf);
         uint32_t sign_of_b = b.v >> 31;
         a.v &= ~(1 << 31);             // clear the sign bit
         a.v = a.v | (sign_of_b << 31); // add the sign of b
         return from_softfloat32(a);
      }
      // float unops
      float _eosio_f32_abs( float af ) {
         float32_t a = to_softfloat32(af);
         a.v &= ~(1 << 31);
         return from_softfloat32(a);
      }
      float _eosio_f32_neg( float af ) {
         float32_t a = to_softfloat32(af);
         uint32_t sign = a.v >> 31;
         a.v &= ~(1 << 31);
         a.v |= (!sign << 31);
         return from_softfloat32(a);
      }
      float _eosio_f32_sqrt( float a ) {
         float32_t ret = ::f32_sqrt( to_softfloat32(a) );
         return from_softfloat32(ret);
      }
      // ceil, floor, trunc and nearest are lifted from libc
      float _eosio_f32_ceil( float af ) {
         float32_t a = to_softfloat32(af);
         int e = (int)(a.v >> 23 & 0xFF) - 0X7F;
         uint32_t m;
         if (e >= 23)
            return af;
         if (e >= 0) {
            m = 0x007FFFFF >> e;
            if ((a.v & m) == 0)
               return af;
            if (a.v >> 31 == 0)
               a.v += m;
            a.v &= ~m;
         } else {
            if (a.v >> 31)
               a.v = 0x80000000; // return -0.0f
            else if (a.v << 1)
               a.v = 0x3F800000; // return 1.0f
         }

         return from_softfloat32(a);
      }
      float _eosio_f32_floor( float af ) {
         float32_t a = to_softfloat32(af);
         int e = (int)(a.v >> 23 & 0xFF) - 0X7F;
         uint32_t m;
         if (e >= 23)
            return af;
         if (e >= 0) {
            m = 0x007FFFFF >> e;
            if ((a.v & m) == 0)
               return af;
            if (a.v >> 31)
               a.v += m;
            a.v &= ~m;
         } else {
            if (a.v >> 31 == 0)
               a.v = 0;
            else if (a.v << 1)
               a.v = 0xBF800000; // return -1.0f
         }
         return from_softfloat32(a);
      }
      float _eosio_f32_trunc( float af ) {
         float32_t a = to_softfloat32(af);
         int e = (int)(a.v >> 23 & 0xff) - 0x7f + 9;
         uint32_t m;
         if (e >= 23 + 9)
            return af;
         if (e < 9)
            e = 1;
         m = -1U >> e;
         if ((a.v & m) == 0)
            return af;
         a.v &= ~m;
         return from_softfloat32(a);
      }
      float _eosio_f32_nearest( float af ) {
         float32_t a = to_softfloat32(af);
         int e = a.v>>23 & 0xff;
         int s = a.v>>31;
         float32_t y;
         if (e >= 0x7f+23)
            return af;
         if (s)
            y = ::f32_add( ::f32_sub( a, float32_t{inv_float_eps} ), float32_t{inv_float_eps} );
         else
            y = ::f32_sub( ::f32_add( a, float32_t{inv_float_eps} ), float32_t{inv_float_eps} );
         if (::f32_eq( y, {0} ) )
            return s ? -0.0f : 0.0f;
         return from_softfloat32(y);
      }

      // float relops
      bool _eosio_f32_eq( float a, float b ) {  return ::f32_eq( to_softfloat32(a), to_softfloat32(b) ); }
      bool _eosio_f32_ne( float a, float b ) { return !::f32_eq( to_softfloat32(a), to_softfloat32(b) ); }
      bool _eosio_f32_lt( float a, float b ) { return ::f32_lt( to_softfloat32(a), to_softfloat32(b) ); }
      bool _eosio_f32_le( float a, float b ) { return ::f32_le( to_softfloat32(a), to_softfloat32(b) ); }
      bool _eosio_f32_gt( float af, float bf ) {
         float32_t a = to_softfloat32(af);
         float32_t b = to_softfloat32(bf);
         if (is_nan(a))
            return false;
         if (is_nan(b))
            return false;
         return !::f32_le( a, b );
      }
      bool _eosio_f32_ge( float af, float bf ) {
         float32_t a = to_softfloat32(af);
         float32_t b = to_softfloat32(bf);
         if (is_nan(a))
            return false;
         if (is_nan(b))
            return false;
         return !::f32_lt( a, b );
      }

      // double binops
      double _eosio_f64_add( double a, double b ) {
         float64_t ret = ::f64_add( to_softfloat64(a), to_softfloat64(b) );
         return from_softfloat64(ret);
      }
      double _eosio_f64_sub( double a, double b ) {
         float64_t ret = ::f64_sub( to_softfloat64(a), to_softfloat64(b) );
         return from_softfloat64(ret);
      }
      double _eosio_f64_div( double a, double b ) {
         float64_t ret = ::f64_div( to_softfloat64(a), to_softfloat64(b) );
         return from_softfloat64(ret);
      }
      double _eosio_f64_mul( double a, double b ) {
         float64_t ret = ::f64_mul( to_softfloat64(a), to_softfloat64(b) );
         return from_softfloat64(ret);
      }
      double _eosio_f64_min( double af, double bf ) {
         float64_t a = to_softfloat64(af);
         float64_t b = to_softfloat64(bf);
         if (is_nan(a))
            return af;
         if (is_nan(b))
            return bf;
         if (f64_sign_bit(a) != f64_sign_bit(b))
            return f64_sign_bit(a) ? af : bf;
         return ::f64_lt( a, b ) ? af : bf;
      }
      double _eosio_f64_max( double af, double bf ) {
         float64_t a = to_softfloat64(af);
         float64_t b = to_softfloat64(bf);
         if (is_nan(a))
            return af;
         if (is_nan(b))
            return bf;
         if (f64_sign_bit(a) != f64_sign_bit(b))
            return f64_sign_bit(a) ? bf : af;
         return ::f64_lt( a, b ) ? bf : af;
      }
      double _eosio_f64_copysign( double af, double bf ) {
         float64_t a = to_softfloat64(af);
         float64_t b = to_softfloat64(bf);
         uint64_t sign_of_b = b.v >> 63;
         a.v &= ~(uint64_t(1) << 63);             // clear the sign bit
         a.v = a.v | (sign_of_b << 63); // add the sign of b
         return from_softfloat64(a);
      }

      // double unops
      double _eosio_f64_abs( double af ) {
         float64_t a = to_softfloat64(af);
         a.v &= ~(uint64_t(1) << 63);
         return from_softfloat64(a);
      }
      double _eosio_f64_neg( double af ) {
         float64_t a = to_softfloat64(af);
         uint64_t sign = a.v >> 63;
         a.v &= ~(uint64_t(1) << 63);
         a.v |= (uint64_t(!sign) << 63);
         return from_softfloat64(a);
      }
      double _eosio_f64_sqrt( double a ) {
         float64_t ret = ::f64_sqrt( to_softfloat64(a) );
         return from_softfloat64(ret);
      }
      // ceil, floor, trunc and nearest are lifted from libc
      double _eosio_f64_ceil( double af ) {
         float64_t a = to_softfloat64( af );
         float64_t ret;
         int e = a.v >> 52 & 0x7ff;
         float64_t y;
         if (e >= 0x3ff+52 || ::f64_eq( a, { 0 } ))
            return af;
         /* y = int(x) - x, where int(x) is an integer neighbor of x */
         if (a.v >> 63)
            y = ::f64_sub( ::f64_add( ::f64_sub( a, float64_t{inv_double_eps} ), float64_t{inv_double_eps} ), a );
         else
            y = ::f64_sub( ::f64_sub( ::f64_add( a, float64_t{inv_double_eps} ), float64_t{inv_double_eps} ), a );
         /* special case because of non-nearest rounding modes */
         if (e <= 0x3ff-1) {
            return a.v >> 63 ? -0.0 : 1.0; //float64_t{0x8000000000000000} : float64_t{0xBE99999A3F800000}; //either -0.0 or 1
         }
         if (::f64_lt( y, to_softfloat64(0) )) {
            ret = ::f64_add( ::f64_add( a, y ), to_softfloat64(1) ); // 0xBE99999A3F800000 } ); // plus 1
            return from_softfloat64(ret);
         }
         ret = ::f64_add( a, y );
         return from_softfloat64(ret);
      }
      double _eosio_f64_floor( double af ) {
         float64_t a = to_softfloat64( af );
         float64_t ret;
         int e = a.v >> 52 & 0x7FF;
         float64_t y;
         if ( a.v == 0x8000000000000000) {
            return af;
         }
         if (e >= 0x3FF+52 || a.v == 0) {
            return af;
         }
         if (a.v >> 63)
            y = ::f64_sub( ::f64_add( ::f64_sub( a, float64_t{inv_double_eps} ), float64_t{inv_double_eps} ), a );
         else
            y = ::f64_sub( ::f64_sub( ::f64_add( a, float64_t{inv_double_eps} ), float64_t{inv_double_eps} ), a );
         if (e <= 0x3FF-1) {
            return a.v>>63 ? -1.0 : 0.0; //float64_t{0xBFF0000000000000} : float64_t{0}; // -1 or 0
         }
         if ( !::f64_le( y, float64_t{0} ) ) {
            ret = ::f64_sub( ::f64_add(a,y), to_softfloat64(1.0));
            return from_softfloat64(ret);
         }
         ret = ::f64_add( a, y );
         return from_softfloat64(ret);
      }
      double _eosio_f64_trunc( double af ) {
         float64_t a = to_softfloat64( af );
         int e = (int)(a.v >> 52 & 0x7ff) - 0x3ff + 12;
         uint64_t m;
         if (e >= 52 + 12)
            return af;
         if (e < 12)
            e = 1;
         m = -1ULL >> e;
         if ((a.v & m) == 0)
            return af;
         a.v &= ~m;
         return from_softfloat64(a);
      }

      double _eosio_f64_nearest( double af ) {
         float64_t a = to_softfloat64( af );
         int e = (a.v >> 52 & 0x7FF);
         int s = a.v >> 63;
         float64_t y;
         if ( e >= 0x3FF+52 )
            return af;
         if ( s )
            y = ::f64_add( ::f64_sub( a, float64_t{inv_double_eps} ), float64_t{inv_double_eps} );
         else
            y = ::f64_sub( ::f64_add( a, float64_t{inv_double_eps} ), float64_t{inv_double_eps} );
         if ( ::f64_eq( y, float64_t{0} ) )
            return s ? -0.0 : 0.0;
         return from_softfloat64(y);
      }

      // double relops
      bool _eosio_f64_eq( double a, double b ) { return ::f64_eq( to_softfloat64(a), to_softfloat64(b) ); }
      bool _eosio_f64_ne( double a, double b ) { return !::f64_eq( to_softfloat64(a), to_softfloat64(b) ); }
      bool _eosio_f64_lt( double a, double b ) { return ::f64_lt( to_softfloat64(a), to_softfloat64(b) ); }
      bool _eosio_f64_le( double a, double b ) { return ::f64_le( to_softfloat64(a), to_softfloat64(b) ); }
      bool _eosio_f64_gt( double af, double bf ) {
         float64_t a = to_softfloat64(af);
         float64_t b = to_softfloat64(bf);
         if (is_nan(a))
            return false;
         if (is_nan(b))
            return false;
         return !::f64_le( a, b );
      }
      bool _eosio_f64_ge( double af, double bf ) {
         float64_t a = to_softfloat64(af);
         float64_t b = to_softfloat64(bf);
         if (is_nan(a))
            return false;
         if (is_nan(b))
            return false;
         return !::f64_lt( a, b );
      }

      // float and double conversions
      double _eosio_f32_promote( float a ) {
         return from_softfloat64(f32_to_f64( to_softfloat32(a)) );
      }
      float _eosio_f64_demote( double a ) {
         return from_softfloat32(f64_to_f32( to_softfloat64(a)) );
      }
      int32_t _eosio_f32_trunc_i32s( float af ) {
         float32_t a = to_softfloat32(af);
         if (_eosio_f32_ge(af, 2147483648.0f) || _eosio_f32_lt(af, -2147483648.0f))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_s/i32 overflow" );

         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_s/i32 unrepresentable");
         return f32_to_i32( to_softfloat32(_eosio_f32_trunc( af )), 0, false );
      }
      int32_t _eosio_f64_trunc_i32s( double af ) {
         float64_t a = to_softfloat64(af);
         if (_eosio_f64_ge(af, 2147483648.0) || _eosio_f64_lt(af, -2147483648.0))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_s/i32 overflow");
         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_s/i32 unrepresentable");
         return f64_to_i32( to_softfloat64(_eosio_f64_trunc( af )), 0, false );
      }
      uint32_t _eosio_f32_trunc_i32u( float af ) {
         float32_t a = to_softfloat32(af);
         if (_eosio_f32_ge(af, 4294967296.0f) || _eosio_f32_le(af, -1.0f))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_u/i32 overflow");
         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_u/i32 unrepresentable");
         return f32_to_ui32( to_softfloat32(_eosio_f32_trunc( af )), 0, false );
      }
      uint32_t _eosio_f64_trunc_i32u( double af ) {
         float64_t a = to_softfloat64(af);
         if (_eosio_f64_ge(af, 4294967296.0) || _eosio_f64_le(af, -1.0))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_u/i32 overflow");
         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_u/i32 unrepresentable");
         return f64_to_ui32( to_softfloat64(_eosio_f64_trunc( af )), 0, false );
      }
      int64_t _eosio_f32_trunc_i64s( float af ) {
         float32_t a = to_softfloat32(af);
         if (_eosio_f32_ge(af, 9223372036854775808.0f) || _eosio_f32_lt(af, -9223372036854775808.0f))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_s/i64 overflow");
         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_s/i64 unrepresentable");
         return f32_to_i64( to_softfloat32(_eosio_f32_trunc( af )), 0, false );
      }
      int64_t _eosio_f64_trunc_i64s( double af ) {
         float64_t a = to_softfloat64(af);
         if (_eosio_f64_ge(af, 9223372036854775808.0) || _eosio_f64_lt(af, -9223372036854775808.0))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_s/i64 overflow");
         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_s/i64 unrepresentable");

         return f64_to_i64( to_softfloat64(_eosio_f64_trunc( af )), 0, false );
      }
      uint64_t _eosio_f32_trunc_i64u( float af ) {
         float32_t a = to_softfloat32(af);
         if (_eosio_f32_ge(af, 18446744073709551616.0f) || _eosio_f32_le(af, -1.0f))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_u/i64 overflow");
         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_u/i64 unrepresentable");
         return f32_to_ui64( to_softfloat32(_eosio_f32_trunc( af )), 0, false );
      }
      uint64_t _eosio_f64_trunc_i64u( double af ) {
         float64_t a = to_softfloat64(af);
         if (_eosio_f64_ge(af, 18446744073709551616.0) || _eosio_f64_le(af, -1.0))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_u/i64 overflow");
         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_u/i64 unrepresentable");
         return f64_to_ui64( to_softfloat64(_eosio_f64_trunc( af )), 0, false );
      }
      float _eosio_i32_to_f32( int32_t a )  {
         return from_softfloat32(i32_to_f32( a ));
      }
      float _eosio_i64_to_f32( int64_t a ) {
         return from_softfloat32(i64_to_f32( a ));
      }
      float _eosio_ui32_to_f32( uint32_t a ) {
         return from_softfloat32(ui32_to_f32( a ));
      }
      float _eosio_ui64_to_f32( uint64_t a ) {
         return from_softfloat32(ui64_to_f32( a ));
      }
      double _eosio_i32_to_f64( int32_t a ) {
         return from_softfloat64(i32_to_f64( a ));
      }
      double _eosio_i64_to_f64( int64_t a ) {
         return from_softfloat64(i64_to_f64( a ));
      }
      double _eosio_ui32_to_f64( uint32_t a ) {
         return from_softfloat64(ui32_to_f64( a ));
      }
      double _eosio_ui64_to_f64( uint64_t a ) {
         return from_softfloat64(ui64_to_f64( a ));
      }

      static bool is_nan( const float32_t f ) {
         return f32_is_nan( f );
      }
      static bool is_nan( const float64_t f ) {
         return f64_is_nan( f );
      }
      static bool is_nan( const float128_t& f ) {
         return f128_is_nan( f );
      }

      static constexpr uint32_t inv_float_eps = 0x4B000000;
      static constexpr uint64_t inv_double_eps = 0x4330000000000000;
};

class compiler_builtins  {
   public:
      compiler_builtins( apply_context& ctx ) {}

      void __ashlti3(__int128& ret, uint64_t low, uint64_t high, uint32_t shift) {
         fc::uint128_t i(high, low);
         i <<= shift;
         ret = (unsigned __int128)i;
      }

      void __ashrti3(__int128& ret, uint64_t low, uint64_t high, uint32_t shift) {
         // retain the signedness
         ret = high;
         ret <<= 64;
         ret |= low;
         ret >>= shift;
      }

      void __lshlti3(__int128& ret, uint64_t low, uint64_t high, uint32_t shift) {
         fc::uint128_t i(high, low);
         i <<= shift;
         ret = (unsigned __int128)i;
      }

      void __lshrti3(__int128& ret, uint64_t low, uint64_t high, uint32_t shift) {
         fc::uint128_t i(high, low);
         i >>= shift;
         ret = (unsigned __int128)i;
      }

      void __divti3(__int128& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
         __int128 lhs = ha;
         __int128 rhs = hb;

         lhs <<= 64;
         lhs |=  la;

         rhs <<= 64;
         rhs |=  lb;

         KOINOS_ASSERT(rhs != 0, arithmetic_exception, "divide by zero");

         lhs /= rhs;

         ret = lhs;
      }

      void __udivti3(unsigned __int128& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
         unsigned __int128 lhs = ha;
         unsigned __int128 rhs = hb;

         lhs <<= 64;
         lhs |=  la;

         rhs <<= 64;
         rhs |=  lb;

         KOINOS_ASSERT(rhs != 0, arithmetic_exception, "divide by zero");

         lhs /= rhs;
         ret = lhs;
      }

      void __multi3(__int128& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
         __int128 lhs = ha;
         __int128 rhs = hb;

         lhs <<= 64;
         lhs |=  la;

         rhs <<= 64;
         rhs |=  lb;

         lhs *= rhs;
         ret = lhs;
      }

      void __modti3(__int128& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
         __int128 lhs = ha;
         __int128 rhs = hb;

         lhs <<= 64;
         lhs |=  la;

         rhs <<= 64;
         rhs |=  lb;

         KOINOS_ASSERT(rhs != 0, arithmetic_exception, "divide by zero");

         lhs %= rhs;
         ret = lhs;
      }

      void __umodti3(unsigned __int128& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
         unsigned __int128 lhs = ha;
         unsigned __int128 rhs = hb;

         lhs <<= 64;
         lhs |=  la;

         rhs <<= 64;
         rhs |=  lb;

         KOINOS_ASSERT(rhs != 0, arithmetic_exception, "divide by zero");

         lhs %= rhs;
         ret = lhs;
      }

      // arithmetic long double
      void __addtf3( float128_t& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb ) {
         float128_t a = {{ la, ha }};
         float128_t b = {{ lb, hb }};
         ret = f128_add( a, b );
      }
      void __subtf3( float128_t& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb ) {
         float128_t a = {{ la, ha }};
         float128_t b = {{ lb, hb }};
         ret = f128_sub( a, b );
      }
      void __multf3( float128_t& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb ) {
         float128_t a = {{ la, ha }};
         float128_t b = {{ lb, hb }};
         ret = f128_mul( a, b );
      }
      void __divtf3( float128_t& ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb ) {
         float128_t a = {{ la, ha }};
         float128_t b = {{ lb, hb }};
         ret = f128_div( a, b );
      }
      void __negtf2( float128_t& ret, uint64_t la, uint64_t ha ) {
         ret = {{ la, (ha ^ (uint64_t)1 << 63) }};
      }

      // conversion long double
      void __extendsftf2( float128_t& ret, float f ) {
         ret = f32_to_f128( to_softfloat32(f) );
      }
      void __extenddftf2( float128_t& ret, double d ) {
         ret = f64_to_f128( to_softfloat64(d) );
      }
      double __trunctfdf2( uint64_t l, uint64_t h ) {
         float128_t f = {{ l, h }};
         return from_softfloat64(f128_to_f64( f ));
      }
      float __trunctfsf2( uint64_t l, uint64_t h ) {
         float128_t f = {{ l, h }};
         return from_softfloat32(f128_to_f32( f ));
      }
      int32_t __fixtfsi( uint64_t l, uint64_t h ) {
         float128_t f = {{ l, h }};
         return f128_to_i32( f, 0, false );
      }
      int64_t __fixtfdi( uint64_t l, uint64_t h ) {
         float128_t f = {{ l, h }};
         return f128_to_i64( f, 0, false );
      }
      void __fixtfti( __int128& ret, uint64_t l, uint64_t h ) {
         float128_t f = {{ l, h }};
         ret = ___fixtfti( f );
      }
      uint32_t __fixunstfsi( uint64_t l, uint64_t h ) {
         float128_t f = {{ l, h }};
         return f128_to_ui32( f, 0, false );
      }
      uint64_t __fixunstfdi( uint64_t l, uint64_t h ) {
         float128_t f = {{ l, h }};
         return f128_to_ui64( f, 0, false );
      }
      void __fixunstfti( unsigned __int128& ret, uint64_t l, uint64_t h ) {
         float128_t f = {{ l, h }};
         ret = ___fixunstfti( f );
      }
      void __fixsfti( __int128& ret, float a ) {
         ret = ___fixsfti( to_softfloat32(a).v );
      }
      void __fixdfti( __int128& ret, double a ) {
         ret = ___fixdfti( to_softfloat64(a).v );
      }
      void __fixunssfti( unsigned __int128& ret, float a ) {
         ret = ___fixunssfti( to_softfloat32(a).v );
      }
      void __fixunsdfti( unsigned __int128& ret, double a ) {
         ret = ___fixunsdfti( to_softfloat64(a).v );
      }
      double __floatsidf( int32_t i ) {
         return from_softfloat64(i32_to_f64(i));
      }
      void __floatsitf( float128_t& ret, int32_t i ) {
         ret = i32_to_f128(i);
      }
      void __floatditf( float128_t& ret, uint64_t a ) {
         ret = i64_to_f128( a );
      }
      void __floatunsitf( float128_t& ret, uint32_t i ) {
         ret = ui32_to_f128(i);
      }
      void __floatunditf( float128_t& ret, uint64_t a ) {
         ret = ui64_to_f128( a );
      }
      double __floattidf( uint64_t l, uint64_t h ) {
         fc::uint128_t v(h, l);
         unsigned __int128 val = (unsigned __int128)v;
         return ___floattidf( *(__int128*)&val );
      }
      double __floatuntidf( uint64_t l, uint64_t h ) {
         fc::uint128_t v(h, l);
         return ___floatuntidf( (unsigned __int128)v );
      }
      int ___cmptf2( uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb, int return_value_if_nan ) {
         float128_t a = {{ la, ha }};
         float128_t b = {{ lb, hb }};
         if ( __unordtf2(la, ha, lb, hb) )
            return return_value_if_nan;
         if ( f128_lt( a, b ) )
            return -1;
         if ( f128_eq( a, b ) )
            return 0;
         return 1;
      }
      int __eqtf2( uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb ) {
         return ___cmptf2(la, ha, lb, hb, 1);
      }
      int __netf2( uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb ) {
         return ___cmptf2(la, ha, lb, hb, 1);
      }
      int __getf2( uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb ) {
         return ___cmptf2(la, ha, lb, hb, -1);
      }
      int __gttf2( uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb ) {
         return ___cmptf2(la, ha, lb, hb, 0);
      }
      int __letf2( uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb ) {
         return ___cmptf2(la, ha, lb, hb, 1);
      }
      int __lttf2( uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb ) {
         return ___cmptf2(la, ha, lb, hb, 0);
      }
      int __cmptf2( uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb ) {
         return ___cmptf2(la, ha, lb, hb, 1);
      }
      int __unordtf2( uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb ) {
         float128_t a = {{ la, ha }};
         float128_t b = {{ lb, hb }};
         if ( softfloat_api::is_nan(a) || softfloat_api::is_nan(b) )
            return 1;
         return 0;
      }

      static constexpr uint32_t SHIFT_WIDTH = (sizeof(uint64_t)*8)-1;
};

} // koinos::chain
