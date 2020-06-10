#include <koinos/chain/system_calls.hpp>

#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/json.hpp>

#include <koinos/log.hpp>

std::string hex_string( const char* d, size_t len )
{
   static const auto hex = "0123456789ABCDEF";

   std::stringstream ss;

   for( int i = 0; i < len; i++)
      ss << hex[(*(d + i) & 0xF0) >> 4] << hex[*(d + i) & 0x0F];

   return ss.str();
}

namespace koinos::chain {

// System Call Table API
//
// The system_call_table keeps track of system calls that have been overridden. It is
// called from the public system call slot automatically defined by the SYSTEM_CALL_DEFINE
// macro.

bool system_call_table::overridable( system_call_slot s ) noexcept
{
   return static_cast< uint32_t >( s ) % 2 == 0;
}

void system_call_table::update()
{
   for ( auto& [ key, value ] : pending_updates )
      system_call_map[ key ] = value;

   pending_updates.clear();
}

std::optional< system_call_bundle > system_call_table::get_system_call( system_call_slot s )
{
   if ( system_call_map.find( s ) != system_call_map.end() )
      return system_call_map[ s ];
   return {};
}

void system_call_table::set_system_call( system_call_slot s, system_call_bundle v )
{
   KOINOS_ASSERT( overridable( s ), system_call_not_overridable, "system call cannot be overridden" );
   pending_updates[ s ] = v;
}

// System API Definitions
//
// Using the SYSTEM_CALL_DEFINE macro will define the public facing system call for you. It will check the
// system_call_table class to see if an override exists and call it. If there is no system call override for
// the given system call it will call the native implement that you define.
//
// The native implementation should check whether or not we are in kernel mode and throw if we are not. This
// will prevent the user mode contract from circumventing the public interface. Use the provided macro
// SYSTEM_CALL_ENFORCE_KERNEL_MODE();

namespace detail {
   struct soft_float
   {
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

   struct compiler_builtins
   {
      static constexpr uint32_t SHIFT_WIDTH = (sizeof(uint64_t)*8)-1;
   };
}

system_api::system_api( apply_context& _ctx ) : context( _ctx ) {}

SYSTEM_CALL_DEFINE( void, abort )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   KOINOS_ASSERT( false, abort_called, "abort() called");
}

SYSTEM_CALL_DEFINE( void, eosio_assert, ((bool) condition, (null_terminated_ptr) msg) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   constexpr size_t max_assert_message = 1024;
   if( BOOST_UNLIKELY( !condition ) )
   {
      const size_t sz = strnlen( msg, max_assert_message );
      std::string message( msg, sz );
      KOINOS_THROW( chain_exception, "assertion failure with message: ${s}", ("s",message) );
   }
}

SYSTEM_CALL_DEFINE( void, eosio_assert_message, ((bool) condition, (array_ptr<const char>) msg, (uint32_t) len) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   constexpr size_t max_assert_message = 1024;
   if( BOOST_UNLIKELY( !condition ) )
   {
      const size_t sz = strnlen( msg, max_assert_message );
      std::string message( msg, sz );
      KOINOS_THROW( chain_exception, "assertion failure with message: ${s}", ("s",message) );
   }
}

SYSTEM_CALL_DEFINE( void, eosio_assert_code, ((bool) condition, (uint64_t) error_code) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
}
SYSTEM_CALL_DEFINE( void, eosio_exit, ((int32_t) code) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
}

SYSTEM_CALL_DEFINE( int, read_action_data, ((array_ptr<char>) memory, (uint32_t) buffer_size) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
//   auto s = ctx.get_action().data.size();
//   if( buffer_size == 0 ) return s;
//
//   auto copy_size = std::min( static_cast<size_t>(buffer_size), s );
//   memcpy( (char*)memory.value, context.get_action().data.data(), copy_size );
//
//   return copy_size;
   return 0;
}

SYSTEM_CALL_DEFINE( int, action_data_size )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return 0;
}

SYSTEM_CALL_DEFINE( name, current_receiver )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return context.receiver;
}

SYSTEM_CALL_DEFINE( char*, memcpy, ((array_ptr< char >) dest, (array_ptr< const char >) src, (uint32_t) length) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   KOINOS_ASSERT((size_t)(std::abs((ptrdiff_t)dest.value - (ptrdiff_t)src.value)) >= length, chain_exception, "memcpy can only accept non-aliasing pointers");
   return (char *)::memcpy(dest, src, length);
}

SYSTEM_CALL_DEFINE( char*, memmove, ((array_ptr< char >) dest, (array_ptr< const char >) src, (uint32_t) length) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return (char *)::memmove(dest, src, length);
}

SYSTEM_CALL_DEFINE( int, memcmp, ((array_ptr< const char >) dest, (array_ptr< const char >) src, (uint32_t) length) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   int ret = ::memcmp(dest, src, length);
   if(ret < 0)
      return -1;
   if(ret > 0)
      return 1;
   return 0;
}

SYSTEM_CALL_DEFINE( char*, memset, ((array_ptr< char >) dest, (int) value, (uint32_t) length) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return (char *)::memset( dest, value, length );
}

SYSTEM_CALL_DEFINE( void, prints, ((null_terminated_ptr) str) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   context.console_append( static_cast< const char* >(str) );
}

SYSTEM_CALL_DEFINE( void, prints_l, ((array_ptr< const char >) str, (uint32_t) str_len ) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   context.console_append(string(str, str_len));
}

SYSTEM_CALL_DEFINE( void, printi, ((int64_t) val) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   std::ostringstream oss;
   oss << val;
   context.console_append( oss.str() );
}

SYSTEM_CALL_DEFINE( void, printui, ((uint64_t) val) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   std::ostringstream oss;
   oss << val;
   context.console_append( oss.str() );
}

SYSTEM_CALL_DEFINE( void, printi128, ((const __int128&) val) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   bool is_negative = (val < 0);
   unsigned __int128 val_magnitude;

   if( is_negative )
      val_magnitude = static_cast<unsigned __int128>(-val); // Works even if val is at the lowest possible value of a int128_t
   else
      val_magnitude = static_cast<unsigned __int128>(val);

   string s;
   if( is_negative ) {
      s += '-';
   }
   nlohmann::json j;
   pack::to_json( j, static_cast< protocol::uint128_t >( val_magnitude ) );
   s += j.dump();

   context.console_append( s );
}

SYSTEM_CALL_DEFINE( void, printui128, ((const unsigned __int128&) val) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   nlohmann::json j;
   pack::to_json( j, static_cast< protocol::uint128_t >(val) );
   context.console_append(j.dump());
}

SYSTEM_CALL_DEFINE( void, printsf, ((float) val) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   // Assumes float representation on native side is the same as on the WASM side
   std::ostringstream oss;
   oss.setf( std::ios::scientific, std::ios::floatfield );
   oss.precision( std::numeric_limits<float>::digits10 );
   oss << val;
   context.console_append( oss.str() );
}

SYSTEM_CALL_DEFINE( void, printdf, ((double) val) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   // Assumes double representation on native side is the same as on the WASM side
   std::ostringstream oss;
   oss.setf( std::ios::scientific, std::ios::floatfield );
   oss.precision( std::numeric_limits<double>::digits10 );
   oss << val;
   context.console_append( oss.str() );
}

SYSTEM_CALL_DEFINE( void, printqf, ((const float128_t&) val) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   /*
    * Native-side long double uses an 80-bit extended-precision floating-point number.
    * The easiest solution for now was to use the Berkeley softfloat library to round the 128-bit
    * quadruple-precision floating-point number to an 80-bit extended-precision floating-point number
    * (losing precision) which then allows us to simply cast it into a long double for printing purposes.
    *
    * Later we might find a better solution to print the full quadruple-precision floating-point number.
    * Maybe with some compilation flag that turns long double into a quadruple-precision floating-point number,
    * or maybe with some library that allows us to print out quadruple-precision floating-point numbers without
    * having to deal with long doubles at all.
    */

   std::ostringstream oss;
   oss.setf( std::ios::scientific, std::ios::floatfield );

#ifdef __x86_64__
   oss.precision( std::numeric_limits<long double>::digits10 );
   extFloat80_t val_approx;
   f128M_to_extF80M(&val, &val_approx);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
   oss << *(long double*)(&val_approx);
#pragma GCC diagnostic pop
#else
   oss.precision( std::numeric_limits<double>::digits10 );
   double val_approx = from_softfloat64( f128M_to_f64(&val) );
   oss << val_approx;
#endif
   context.console_append( oss.str() );
}

SYSTEM_CALL_DEFINE( void, printn, ((name) value) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   context.console_append(value.to_string());
}

SYSTEM_CALL_DEFINE( void, printhex, ((array_ptr< const char >) data, (uint32_t) data_len ) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   context.console_append(hex_string(data, data_len));
}

SYSTEM_CALL_DEFINE( int, db_store_i64, ((uint64_t) scope, (uint64_t) table, (uint64_t) payer, (uint64_t) id, (array_ptr<const char>) buffer, (uint32_t) buffer_size) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return context.db_store_i64( name(scope), name(table), account_name(payer), id, buffer, buffer_size );
}

SYSTEM_CALL_DEFINE( void, db_update_i64, ((int) itr, (uint64_t) payer, (array_ptr< const char >) buffer, (uint32_t) buffer_size) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   context.db_update_i64( itr, account_name(payer), buffer, buffer_size );
}

SYSTEM_CALL_DEFINE( void, db_remove_i64, ((int) itr) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   context.db_remove_i64( itr );
}

SYSTEM_CALL_DEFINE( int, db_get_i64, ((int) itr, (array_ptr< char >) buffer, (uint32_t) buffer_size) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return context.db_get_i64( itr, buffer, buffer_size );
}

SYSTEM_CALL_DEFINE( int, db_next_i64, ((int) itr, (uint64_t&) primary) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return context.db_next_i64(itr, primary);
}

SYSTEM_CALL_DEFINE( int, db_previous_i64, ((int) itr, (uint64_t&) primary) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return context.db_previous_i64(itr, primary);
}

SYSTEM_CALL_DEFINE( int, db_find_i64, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (uint64_t) id) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return context.db_find_i64( name(code), name(scope), name(table), id );
}

SYSTEM_CALL_DEFINE( int, db_lowerbound_i64, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (uint64_t) id) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return context.db_lowerbound_i64( name(code), name(scope), name(table), id );
}

SYSTEM_CALL_DEFINE( int, db_upperbound_i64, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (uint64_t) id) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return context.db_upperbound_i64( name(code), name(scope), name(table), id );
}

SYSTEM_CALL_DEFINE( int, db_end_i64, ((uint64_t) code, (uint64_t) scope, (uint64_t) table) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return context.db_end_i64( name(code), name(scope), name(table) );
}

SYSTEM_CALL_DB_WRAPPERS_SIMPLE_SECONDARY(idx64,  uint64_t)
SYSTEM_CALL_DB_WRAPPERS_SIMPLE_SECONDARY(idx128, uint128_t)
SYSTEM_CALL_DB_WRAPPERS_ARRAY_SECONDARY(idx256, 2, uint128_t)
SYSTEM_CALL_DB_WRAPPERS_FLOAT_SECONDARY(idx_double, float64_t)
SYSTEM_CALL_DB_WRAPPERS_FLOAT_SECONDARY(idx_long_double, float128_t)


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
      // float binops
SYSTEM_CALL_DEFINE( float, _eosio_f32_add, ((float) a, (float) b ) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t ret = ::f32_add( to_softfloat32(a), to_softfloat32(b) );
   return *reinterpret_cast<float*>(&ret);
}

SYSTEM_CALL_DEFINE( float, _eosio_f32_sub, ((float) a, (float) b ) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t ret = ::f32_sub( to_softfloat32(a), to_softfloat32(b) );
   return *reinterpret_cast<float*>(&ret);
}

SYSTEM_CALL_DEFINE( float, _eosio_f32_div, ((float) a, (float) b ) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t ret = ::f32_div( to_softfloat32(a), to_softfloat32(b) );
   return *reinterpret_cast<float*>(&ret);
}

SYSTEM_CALL_DEFINE( float, _eosio_f32_mul, ((float) a, (float) b) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t ret = ::f32_mul( to_softfloat32(a), to_softfloat32(b) );
   return *reinterpret_cast<float*>(&ret);
}
#pragma GCC diagnostic pop

SYSTEM_CALL_DEFINE( float, _eosio_f32_min, ((float) af, (float) bf) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t a = to_softfloat32(af);
   float32_t b = to_softfloat32(bf);
   if (detail::soft_float::is_nan(a)) {
      return af;
   }
   if (detail::soft_float::is_nan(b)) {
      return bf;
   }
   if ( f32_sign_bit(a) != f32_sign_bit(b) ) {
      return f32_sign_bit(a) ? af : bf;
   }
   return ::f32_lt(a,b) ? af : bf;
}

SYSTEM_CALL_DEFINE( float, _eosio_f32_max, ((float) af, (float) bf) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t a = to_softfloat32(af);
   float32_t b = to_softfloat32(bf);
   if (detail::soft_float::is_nan(a)) {
      return af;
   }
   if (detail::soft_float::is_nan(b)) {
      return bf;
   }
   if ( f32_sign_bit(a) != f32_sign_bit(b) ) {
      return f32_sign_bit(a) ? bf : af;
   }
   return ::f32_lt( a, b ) ? bf : af;
}

SYSTEM_CALL_DEFINE( float, _eosio_f32_copysign, ((float) af, (float) bf) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t a = to_softfloat32(af);
   float32_t b = to_softfloat32(bf);
   uint32_t sign_of_b = b.v >> 31;
   a.v &= ~(1 << 31);             // clear the sign bit
   a.v = a.v | (sign_of_b << 31); // add the sign of b
   return from_softfloat32(a);
}


SYSTEM_CALL_DEFINE( float, _eosio_f32_abs, ((float) af ) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t a = to_softfloat32(af);
   a.v &= ~(1 << 31);
   return from_softfloat32(a);
}

SYSTEM_CALL_DEFINE( float, _eosio_f32_neg, ((float) af ) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t a = to_softfloat32(af);
   uint32_t sign = a.v >> 31;
   a.v &= ~(1 << 31);
   a.v |= (!sign << 31);
   return from_softfloat32(a);
}

SYSTEM_CALL_DEFINE( float, _eosio_f32_sqrt, ((float) a ) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t ret = ::f32_sqrt( to_softfloat32(a) );
   return from_softfloat32(ret);
}

SYSTEM_CALL_DEFINE( float, _eosio_f32_ceil, ((float) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
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

SYSTEM_CALL_DEFINE( float, _eosio_f32_floor, ((float) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
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

SYSTEM_CALL_DEFINE( float, _eosio_f32_trunc, ((float) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
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

SYSTEM_CALL_DEFINE( float, _eosio_f32_nearest, ((float) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t a = to_softfloat32(af);
   int e = a.v>>23 & 0xff;
   int s = a.v>>31;
   float32_t y;
   if (e >= 0x7f+23)
      return af;
   if (s)
      y = ::f32_add( ::f32_sub( a, float32_t{detail::soft_float::inv_float_eps} ), float32_t{detail::soft_float::inv_float_eps} );
   else
      y = ::f32_sub( ::f32_add( a, float32_t{detail::soft_float::inv_float_eps} ), float32_t{detail::soft_float::inv_float_eps} );
   if (::f32_eq( y, {0} ) )
      return s ? -0.0f : 0.0f;
   return from_softfloat32(y);
}

SYSTEM_CALL_DEFINE( bool, _eosio_f32_eq, ((float) a, (float) b) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return ::f32_eq( to_softfloat32(a), to_softfloat32(b) );
}

SYSTEM_CALL_DEFINE( bool, _eosio_f32_ne, ((float) a, (float) b) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return !::f32_eq( to_softfloat32(a), to_softfloat32(b) );
}

SYSTEM_CALL_DEFINE( bool, _eosio_f32_lt, ((float) a, (float) b) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return ::f32_lt( to_softfloat32(a), to_softfloat32(b) );
}

SYSTEM_CALL_DEFINE( bool, _eosio_f32_le, ((float) a, (float) b) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return ::f32_le( to_softfloat32(a), to_softfloat32(b) );
}

SYSTEM_CALL_DEFINE( bool, _eosio_f32_gt, ((float) af, (float) bf) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t a = to_softfloat32(af);
   float32_t b = to_softfloat32(bf);
   if (detail::soft_float::is_nan(a))
      return false;
   if (detail::soft_float::is_nan(b))
      return false;
   return !::f32_le( a, b );
}

SYSTEM_CALL_DEFINE( bool, _eosio_f32_ge, ((float) af, (float) bf) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t a = to_softfloat32(af);
   float32_t b = to_softfloat32(bf);
   if (detail::soft_float::is_nan(a))
      return false;
   if (detail::soft_float::is_nan(b))
      return false;
   return !::f32_lt( a, b );
}

SYSTEM_CALL_DEFINE( double, _eosio_f64_add,((double) a, (double) b ) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t ret = ::f64_add( to_softfloat64(a), to_softfloat64(b) );
   return from_softfloat64(ret);
}

SYSTEM_CALL_DEFINE( double, _eosio_f64_sub, ((double) a, (double) b) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t ret = ::f64_sub( to_softfloat64(a), to_softfloat64(b) );
   return from_softfloat64(ret);
}

SYSTEM_CALL_DEFINE( double, _eosio_f64_div, ((double) a, (double) b) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t ret = ::f64_div( to_softfloat64(a), to_softfloat64(b) );
   return from_softfloat64(ret);
}

SYSTEM_CALL_DEFINE( double, _eosio_f64_mul, ((double) a, (double) b) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t ret = ::f64_mul( to_softfloat64(a), to_softfloat64(b) );
   return from_softfloat64(ret);
}

SYSTEM_CALL_DEFINE( double, _eosio_f64_min, ((double) af, (double) bf) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t a = to_softfloat64(af);
   float64_t b = to_softfloat64(bf);
   if (detail::soft_float::is_nan(a))
      return af;
   if (detail::soft_float::is_nan(b))
      return bf;
   if (f64_sign_bit(a) != f64_sign_bit(b))
      return f64_sign_bit(a) ? af : bf;
   return ::f64_lt( a, b ) ? af : bf;
}

SYSTEM_CALL_DEFINE( double, _eosio_f64_max, ((double) af, (double) bf) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t a = to_softfloat64(af);
   float64_t b = to_softfloat64(bf);
   if (detail::soft_float::is_nan(a))
      return af;
   if (detail::soft_float::is_nan(b))
      return bf;
   if (f64_sign_bit(a) != f64_sign_bit(b))
      return f64_sign_bit(a) ? bf : af;
   return ::f64_lt( a, b ) ? bf : af;
}

SYSTEM_CALL_DEFINE( double, _eosio_f64_copysign, ((double) af, (double) bf) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t a = to_softfloat64(af);
   float64_t b = to_softfloat64(bf);
   uint64_t sign_of_b = b.v >> 63;
   a.v &= ~(uint64_t(1) << 63);             // clear the sign bit
   a.v = a.v | (sign_of_b << 63); // add the sign of b
   return from_softfloat64(a);
}

SYSTEM_CALL_DEFINE( double, _eosio_f64_abs, ((double) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t a = to_softfloat64(af);
   a.v &= ~(uint64_t(1) << 63);
   return from_softfloat64(a);
}

SYSTEM_CALL_DEFINE( double, _eosio_f64_neg, ((double) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t a = to_softfloat64(af);
   uint64_t sign = a.v >> 63;
   a.v &= ~(uint64_t(1) << 63);
   a.v |= (uint64_t(!sign) << 63);
   return from_softfloat64(a);
}

SYSTEM_CALL_DEFINE( double, _eosio_f64_sqrt, ((double) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t ret = ::f64_sqrt( to_softfloat64(a) );
   return from_softfloat64(ret);
}

SYSTEM_CALL_DEFINE( double, _eosio_f64_ceil, ((double) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t a = to_softfloat64( af );
   float64_t ret;
   int e = a.v >> 52 & 0x7ff;
   float64_t y;
   if (e >= 0x3ff+52 || ::f64_eq( a, { 0 } ))
      return af;
   /* y = int(x) - x, where int(x) is an integer neighbor of x */
   if (a.v >> 63)
      y = ::f64_sub( ::f64_add( ::f64_sub( a, float64_t{detail::soft_float::inv_double_eps} ), float64_t{detail::soft_float::inv_double_eps} ), a );
   else
      y = ::f64_sub( ::f64_sub( ::f64_add( a, float64_t{detail::soft_float::inv_double_eps} ), float64_t{detail::soft_float::inv_double_eps} ), a );
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

SYSTEM_CALL_DEFINE( double, _eosio_f64_floor, ((double) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
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
      y = ::f64_sub( ::f64_add( ::f64_sub( a, float64_t{detail::soft_float::inv_double_eps} ), float64_t{detail::soft_float::inv_double_eps} ), a );
   else
      y = ::f64_sub( ::f64_sub( ::f64_add( a, float64_t{detail::soft_float::inv_double_eps} ), float64_t{detail::soft_float::inv_double_eps} ), a );
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

SYSTEM_CALL_DEFINE( double, _eosio_f64_trunc, ((double) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
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

SYSTEM_CALL_DEFINE( double, _eosio_f64_nearest, ((double) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t a = to_softfloat64( af );
   int e = (a.v >> 52 & 0x7FF);
   int s = a.v >> 63;
   float64_t y;
   if ( e >= 0x3FF+52 )
      return af;
   if ( s )
      y = ::f64_add( ::f64_sub( a, float64_t{detail::soft_float::inv_double_eps} ), float64_t{detail::soft_float::inv_double_eps} );
   else
      y = ::f64_sub( ::f64_add( a, float64_t{detail::soft_float::inv_double_eps} ), float64_t{detail::soft_float::inv_double_eps} );
   if ( ::f64_eq( y, float64_t{0} ) )
      return s ? -0.0 : 0.0;
   return from_softfloat64(y);
}


SYSTEM_CALL_DEFINE( bool, _eosio_f64_eq, ((double) a, (double) b) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return ::f64_eq( to_softfloat64(a), to_softfloat64(b) );
}

SYSTEM_CALL_DEFINE( bool, _eosio_f64_ne, ((double) a, (double) b) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return !::f64_eq( to_softfloat64(a), to_softfloat64(b) );
}

SYSTEM_CALL_DEFINE( bool, _eosio_f64_lt, ((double) a, (double) b) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return ::f64_lt( to_softfloat64(a), to_softfloat64(b) );
}

SYSTEM_CALL_DEFINE( bool, _eosio_f64_le, ((double) a, (double) b) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return ::f64_le( to_softfloat64(a), to_softfloat64(b) );
}

SYSTEM_CALL_DEFINE( bool, _eosio_f64_gt, ((double) af, (double) bf) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t a = to_softfloat64(af);
   float64_t b = to_softfloat64(bf);
   if (detail::soft_float::is_nan(a))
      return false;
   if (detail::soft_float::is_nan(b))
      return false;
   return !::f64_le( a, b );
}

SYSTEM_CALL_DEFINE( bool, _eosio_f64_ge, ((double) af, (double) bf) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t a = to_softfloat64(af);
   float64_t b = to_softfloat64(bf);
   if (detail::soft_float::is_nan(a))
      return false;
   if (detail::soft_float::is_nan(b))
      return false;
   return !::f64_lt( a, b );
}

SYSTEM_CALL_DEFINE( double, _eosio_f32_promote, ((float) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return from_softfloat64(f32_to_f64( to_softfloat32(a)) );
}

SYSTEM_CALL_DEFINE( float, _eosio_f64_demote, ((double) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return from_softfloat32(f64_to_f32( to_softfloat64(a)) );
}

SYSTEM_CALL_DEFINE( int32_t, _eosio_f32_trunc_i32s, ((float) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t a = to_softfloat32(af);
   if (_eosio_f32_ge(af, 2147483648.0f) || _eosio_f32_lt(af, -2147483648.0f))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_s/i32 overflow" );

   if (detail::soft_float::is_nan(a))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_s/i32 unrepresentable");
   return f32_to_i32( to_softfloat32(_eosio_f32_trunc( af )), 0, false );
}

SYSTEM_CALL_DEFINE( int32_t, _eosio_f64_trunc_i32s, ((double) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t a = to_softfloat64(af);
   if (_eosio_f64_ge(af, 2147483648.0) || _eosio_f64_lt(af, -2147483648.0))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_s/i32 overflow");
   if (detail::soft_float::is_nan(a))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_s/i32 unrepresentable");
   return f64_to_i32( to_softfloat64(_eosio_f64_trunc( af )), 0, false );
}

SYSTEM_CALL_DEFINE( uint32_t, _eosio_f32_trunc_i32u, ((float) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t a = to_softfloat32(af);
   if (_eosio_f32_ge(af, 4294967296.0f) || _eosio_f32_le(af, -1.0f))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_u/i32 overflow");
   if (detail::soft_float::is_nan(a))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_u/i32 unrepresentable");
   return f32_to_ui32( to_softfloat32(_eosio_f32_trunc( af )), 0, false );
}

SYSTEM_CALL_DEFINE( uint32_t, _eosio_f64_trunc_i32u, ((double) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t a = to_softfloat64(af);
   if (_eosio_f64_ge(af, 4294967296.0) || _eosio_f64_le(af, -1.0))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_u/i32 overflow");
   if (detail::soft_float::is_nan(a))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_u/i32 unrepresentable");
   return f64_to_ui32( to_softfloat64(_eosio_f64_trunc( af )), 0, false );
}

SYSTEM_CALL_DEFINE( int64_t, _eosio_f32_trunc_i64s, ((float) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t a = to_softfloat32(af);
   if (_eosio_f32_ge(af, 9223372036854775808.0f) || _eosio_f32_lt(af, -9223372036854775808.0f))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_s/i64 overflow");
   if (detail::soft_float::is_nan(a))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_s/i64 unrepresentable");
   return f32_to_i64( to_softfloat32(_eosio_f32_trunc( af )), 0, false );
}

SYSTEM_CALL_DEFINE( int64_t, _eosio_f64_trunc_i64s, ((double) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t a = to_softfloat64(af);
   if (_eosio_f64_ge(af, 9223372036854775808.0) || _eosio_f64_lt(af, -9223372036854775808.0))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_s/i64 overflow");
   if (detail::soft_float::is_nan(a))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_s/i64 unrepresentable");

   return f64_to_i64( to_softfloat64(_eosio_f64_trunc( af )), 0, false );
}

SYSTEM_CALL_DEFINE( uint64_t, _eosio_f32_trunc_i64u, ((float) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float32_t a = to_softfloat32(af);
   if (_eosio_f32_ge(af, 18446744073709551616.0f) || _eosio_f32_le(af, -1.0f))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_u/i64 overflow");
   if (detail::soft_float::is_nan(a))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_u/i64 unrepresentable");
   return f32_to_ui64( to_softfloat32(_eosio_f32_trunc( af )), 0, false );
}

SYSTEM_CALL_DEFINE( uint64_t, _eosio_f64_trunc_i64u, ((double) af) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float64_t a = to_softfloat64(af);
   if (_eosio_f64_ge(af, 18446744073709551616.0) || _eosio_f64_le(af, -1.0))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_u/i64 overflow");
   if (detail::soft_float::is_nan(a))
      KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_u/i64 unrepresentable");
   return f64_to_ui64( to_softfloat64(_eosio_f64_trunc( af )), 0, false );
}

SYSTEM_CALL_DEFINE( float, _eosio_i32_to_f32, ((int32_t) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return from_softfloat32(i32_to_f32( a ));
}

SYSTEM_CALL_DEFINE( float, _eosio_i64_to_f32, ((int64_t) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return from_softfloat32(i64_to_f32( a ));
}

SYSTEM_CALL_DEFINE( float, _eosio_ui32_to_f32, ((uint32_t) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return from_softfloat32(ui32_to_f32( a ));
}

SYSTEM_CALL_DEFINE( float, _eosio_ui64_to_f32, ((uint64_t) a ) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return from_softfloat32(ui64_to_f32( a ));
}

SYSTEM_CALL_DEFINE( double, _eosio_i32_to_f64, ((int32_t) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return from_softfloat64(i32_to_f64( a ));
}

SYSTEM_CALL_DEFINE( double, _eosio_i64_to_f64, ((int64_t) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return from_softfloat64(i64_to_f64( a ));
}

SYSTEM_CALL_DEFINE( double, _eosio_ui32_to_f64, ((uint32_t) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return from_softfloat64(ui32_to_f64( a ));
}

SYSTEM_CALL_DEFINE( double, _eosio_ui64_to_f64, ((uint64_t) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return from_softfloat64(ui64_to_f64( a ));
}

SYSTEM_CALL_DEFINE( void, __ashlti3, ((__int128&) ret, (uint64_t) low, (uint64_t) high, (uint32_t) shift) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   protocol::int128_t i;
   i = high;
   i <<= 64;
   i += low;
   i <<= shift;
   ret = (unsigned __int128)i;
}

SYSTEM_CALL_DEFINE( void, __ashrti3, ((__int128&) ret, (uint64_t) low, (uint64_t) high, (uint32_t) shift) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   // retain the signedness
   ret = high;
   ret <<= 64;
   ret |= low;
   ret >>= shift;
}

SYSTEM_CALL_DEFINE( void, __lshlti3, ((__int128&) ret, (uint64_t) low, (uint64_t) high, (uint32_t) shift) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   protocol::int128_t i;
   i = high;
   i <<= 64;
   i += low;
   i <<= shift;
   ret = (unsigned __int128)i;
}

SYSTEM_CALL_DEFINE( void, __lshrti3, ((__int128&) ret, (uint64_t) low, (uint64_t) high, (uint32_t) shift) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   protocol::uint128_t i;
   i = high;
   i <<= 64;
   i += low;
   i >>= shift;
   ret = (unsigned __int128)i;
}

SYSTEM_CALL_DEFINE( void, __divti3, ((__int128&) ret, (uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
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

SYSTEM_CALL_DEFINE( void, __udivti3, ((unsigned __int128&) ret, (uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
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

SYSTEM_CALL_DEFINE( void, __multi3, ((__int128&) ret, (uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   __int128 lhs = ha;
   __int128 rhs = hb;

   lhs <<= 64;
   lhs |=  la;

   rhs <<= 64;
   rhs |=  lb;

   lhs *= rhs;
   ret = lhs;
}

SYSTEM_CALL_DEFINE( void, __modti3, ((__int128&) ret, (uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
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

SYSTEM_CALL_DEFINE( void, __umodti3, ((unsigned __int128&) ret, (uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
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

SYSTEM_CALL_DEFINE( void, __addtf3, ((float128_t&) ret, (uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float128_t a = {{ la, ha }};
   float128_t b = {{ lb, hb }};
   ret = f128_add( a, b );
}

SYSTEM_CALL_DEFINE( void, __subtf3, ((float128_t&) ret, (uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float128_t a = {{ la, ha }};
   float128_t b = {{ lb, hb }};
   ret = f128_sub( a, b );
}

SYSTEM_CALL_DEFINE( void, __multf3, ((float128_t&) ret, (uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float128_t a = {{ la, ha }};
   float128_t b = {{ lb, hb }};
   ret = f128_mul( a, b );
}

SYSTEM_CALL_DEFINE( void, __divtf3, ((float128_t&) ret, (uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float128_t a = {{ la, ha }};
   float128_t b = {{ lb, hb }};
   ret = f128_div( a, b );
}

SYSTEM_CALL_DEFINE( void, __negtf2, ((float128_t&) ret, (uint64_t) la, (uint64_t) ha) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   ret = {{ la, (ha ^ (uint64_t)1 << 63) }};
}

SYSTEM_CALL_DEFINE( void, __extendsftf2, ((float128_t&) ret, (float) f) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   ret = f32_to_f128( to_softfloat32(f) );
}

SYSTEM_CALL_DEFINE( void, __extenddftf2, ((float128_t&) ret, (double) d) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   ret = f64_to_f128( to_softfloat64(d) );
}

SYSTEM_CALL_DEFINE( double, __trunctfdf2, ((uint64_t) l, (uint64_t) h) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float128_t f = {{ l, h }};
   return from_softfloat64(f128_to_f64( f ));
}

SYSTEM_CALL_DEFINE( float, __trunctfsf2, ((uint64_t) l, (uint64_t) h) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float128_t f = {{ l, h }};
   return from_softfloat32(f128_to_f32( f ));
}

SYSTEM_CALL_DEFINE( int32_t, __fixtfsi, ((uint64_t) l, (uint64_t) h) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float128_t f = {{ l, h }};
   return f128_to_i32( f, 0, false );
}

SYSTEM_CALL_DEFINE( int64_t, __fixtfdi, ((uint64_t) l, (uint64_t) h) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float128_t f = {{ l, h }};
   return f128_to_i64( f, 0, false );
}

SYSTEM_CALL_DEFINE( void, __fixtfti, ((__int128&) ret, (uint64_t) l, (uint64_t) h) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float128_t f = {{ l, h }};
   ret = ___fixtfti( f );
}

SYSTEM_CALL_DEFINE( uint32_t, __fixunstfsi, ((uint64_t) l, (uint64_t) h) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float128_t f = {{ l, h }};
   return f128_to_ui32( f, 0, false );
}

SYSTEM_CALL_DEFINE( uint64_t, __fixunstfdi, ((uint64_t) l, (uint64_t) h) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float128_t f = {{ l, h }};
   return f128_to_ui64( f, 0, false );
}

SYSTEM_CALL_DEFINE( void, __fixunstfti, ((unsigned __int128&) ret, (uint64_t) l, (uint64_t) h) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float128_t f = {{ l, h }};
   ret = ___fixunstfti( f );
}

SYSTEM_CALL_DEFINE( void, __fixsfti, ((__int128&) ret, (float) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   ret = ___fixsfti( to_softfloat32(a).v );
}

SYSTEM_CALL_DEFINE( void, __fixdfti, ((__int128&) ret, (double) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   ret = ___fixdfti( to_softfloat64(a).v );
}

SYSTEM_CALL_DEFINE( void, __fixunssfti, ((unsigned __int128&) ret, (float) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   ret = ___fixunssfti( to_softfloat32(a).v );
}

SYSTEM_CALL_DEFINE( void, __fixunsdfti, ((unsigned __int128&) ret, (double) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   ret = ___fixunsdfti( to_softfloat64(a).v );
}

SYSTEM_CALL_DEFINE( double, __floatsidf, ((int32_t) i) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return from_softfloat64(i32_to_f64(i));
}

SYSTEM_CALL_DEFINE( void, __floatsitf, ((float128_t&) ret, (int32_t) i) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   ret = i32_to_f128(i);
}

SYSTEM_CALL_DEFINE( void, __floatditf, ((float128_t&) ret, (uint64_t) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   ret = i64_to_f128( a );
}

SYSTEM_CALL_DEFINE( void, __floatunsitf, ((float128_t&) ret, (uint32_t) i) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   ret = ui32_to_f128(i);
}

SYSTEM_CALL_DEFINE( void, __floatunditf, ((float128_t&) ret, (uint64_t) a) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   ret = ui64_to_f128( a );
}

SYSTEM_CALL_DEFINE( double, __floattidf, ((uint64_t) l, (uint64_t) h) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   protocol::int128_t i;
   i = h;
   i <<= 64;
   i += l;
   unsigned __int128 val = (unsigned __int128)i;
   return ___floattidf( *(__int128*)&val );
}

SYSTEM_CALL_DEFINE( double, __floatuntidf, ((uint64_t) l, (uint64_t) h) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   protocol::int128_t i;
   i = h;
   i <<= 64;
   i += l;
   return ___floatuntidf( (unsigned __int128)i );
}

SYSTEM_CALL_DEFINE( int, ___cmptf2, ((uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb, (int) return_value_if_nan) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
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

SYSTEM_CALL_DEFINE( int, __eqtf2, ((uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return ___cmptf2(la, ha, lb, hb, 1);
}

SYSTEM_CALL_DEFINE( int, __netf2, ((uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return ___cmptf2(la, ha, lb, hb, 1);
}

SYSTEM_CALL_DEFINE( int, __getf2, ((uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return ___cmptf2(la, ha, lb, hb, -1);
}

SYSTEM_CALL_DEFINE( int, __gttf2, ((uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return ___cmptf2(la, ha, lb, hb, 0);
}

SYSTEM_CALL_DEFINE( int, __letf2, ((uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return ___cmptf2(la, ha, lb, hb, 1);
}

SYSTEM_CALL_DEFINE( int, __lttf2, ((uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return ___cmptf2(la, ha, lb, hb, 0);
}

SYSTEM_CALL_DEFINE( int, __cmptf2, ((uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   return ___cmptf2(la, ha, lb, hb, 1);
}

SYSTEM_CALL_DEFINE( int, __unordtf2, ((uint64_t) la, (uint64_t) ha, (uint64_t) lb, (uint64_t) hb) )
{
   SYSTEM_CALL_ENFORCE_KERNEL_MODE();
   float128_t a = {{ la, ha }};
   float128_t b = {{ lb, hb }};
   if ( detail::soft_float::is_nan(a) || detail::soft_float::is_nan(b) )
      return 1;
   return 0;
}

SYSTEM_CALL_DEFINE( bool, verify_block_header, ((const crypto::recoverable_signature&) sig, (const crypto::multihash_type&) digest) )
{
   return crypto::public_key::from_base58( "5evxVPukp6bUdGNX8XUMD9e2J59j9PjqAVw2xYNw5xrdQPRRT8" ) == crypto::public_key::recover( sig, digest );
}

SYSTEM_CALL_DEFINE( void, apply_block, ((const protocol::active_block_data&) b) )
{

}

SYSTEM_CALL_DEFINE( void, apply_transaction, ((const protocol::transaction_type&) t) )
{

}

SYSTEM_CALL_DEFINE( void, apply_upload_contract_operation, ((const protocol::create_system_contract_operation&) o) )
{

}

SYSTEM_CALL_DEFINE( void, apply_execute_contract_operation, ((const protocol::contract_call_operation&) o) )
{

}

SYSTEM_CALL_DEFINE( bool, db_put_object, ((const statedb::object_space&) space, (const statedb::object_key&) key, (const vl_blob&) obj) )
{
   return false;
}

SYSTEM_CALL_DEFINE( vl_blob, db_get_object, ((const statedb::object_space&) space, (const statedb::object_key&) key) )
{
   return vl_blob();
}

SYSTEM_CALL_DEFINE( vl_blob, db_get_next_object, ((const statedb::object_space&) space, (const statedb::object_key&) key) )
{
   return vl_blob();
}

SYSTEM_CALL_DEFINE( vl_blob, db_get_prev_object, ((const statedb::object_space&) space, (const statedb::object_key&) key) )
{
   return vl_blob();
}

} // koinos::chain
